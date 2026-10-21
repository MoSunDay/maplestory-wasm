#include "UICashShop.h"

#include "../UI.h"
#include "../UITypes/UINotice.h"
#include "../../Data/EquipData.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"

#include <algorithm>

namespace jrc
{
    namespace view = cash_shop_view;

    namespace
    {
        const char* gender_name(int8_t gender)
        {
            return gender == 0 ? "男" : gender == 1 ? "女" :
                gender == 2 ? "全" : "共享";
        }

        std::string period_label(int32_t period)
        {
            return period > 0 ? std::to_string(period) + "天" : "永久";
        }

        const CashCommodity* find_commodity(
            const std::vector<CashCommodity>& catalog, int32_t sn)
        {
            const auto found = std::find_if(catalog.begin(), catalog.end(),
                [sn](const CashCommodity& item) { return item.sn == sn; });
            return found != catalog.end() ? &*found : nullptr;
        }
    }

    void UICashShop::set_balances(CashBalances balances)
    {
        model->set_balances(balances);
        refresh();
    }

    void UICashShop::replace_locker(std::vector<CashLockerItem> items)
    {
        model->replace_locker(std::move(items));
        right_page = view::clamp_page(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        selected_right.reset();
        refresh();
    }

    void UICashShop::purchase_succeeded(CashLockerItem item)
    {
        const int64_t cash_id = item.cash_id;
        model->add_locker(std::move(item));
        set_pending(false);
        select_new_locker_item(cash_id);
    }

    void UICashShop::package_purchase_succeeded(std::vector<CashLockerItem> items)
    {
        int64_t last_cash_id = 0;
        for (auto& item : items)
        {
            last_cash_id = item.cash_id;
            model->add_locker(std::move(item));
        }
        set_pending(false);
        select_new_locker_item(last_cash_id);
    }

    void UICashShop::take_succeeded()
    {
        model->remove_locker(pending_cash_id);
        set_pending(false);
        select_inventory_item(pending_cash_id);
    }

    void UICashShop::put_succeeded(CashLockerItem item)
    {
        const int64_t cash_id = item.cash_id;
        Stage::get().get_player().get_inventory().remove_cash_item(pending_cash_id);
        model->add_locker(std::move(item));
        set_pending(false);
        select_new_locker_item(cash_id);
    }

    void UICashShop::doubleclick(Point<int16_t> cursorpos)
    {
        if (pending)
            return;
        for (uint16_t id = BT_SLOT_FIRST; id < BT_SLOT_LAST; ++id)
        {
            const auto found = buttons.find(id);
            if (found == buttons.end() || !found->second->is_active() ||
                !found->second->bounds(position).contains(cursorpos))
                continue;
            const size_t index = right_page * view::RIGHT_PAGE_SIZE +
                (id - BT_SLOT_FIRST);
            if (index < right_count())
                request_transfer(index);
            return;
        }
    }

    void UICashShop::show_error(const std::string& message)
    {
        set_pending(false);
        UI::get().emplace<UIOk>(message, []() {});
    }

    void UICashShop::reconnect_failed(const std::string& address)
    {
        UI::get().enable();
        show_error("返回游戏服务器失败：" + address);
    }

    void UICashShop::rebuild_catalog()
    {
        filtered = view::filter_catalog(model->catalog(),
            { female, category, search.get_text(),
              category == view::EQUIP_CATEGORY ? equip_sub : 0 });
        left_page = 0;
        selected_left.reset();
        refresh();
    }

    void UICashShop::press_equip_menu(uint16_t id)
    {
        if (id == BT_EQSUB_CLOSE)
        {
            equip_menu_open = false;
            refresh();
            return;
        }
        const int8_t sub = static_cast<int8_t>(id - BT_EQSUB_FIRST);
        // Picking a sub-category folds the flyout away so the catalog grid
        // underneath becomes clickable again.
        equip_menu_open = false;
        if (sub != equip_sub)
        {
            equip_sub = sub;
            rebuild_catalog();
        }
        else
        {
            refresh();
        }
    }

    std::optional<size_t> UICashShop::highlighted_index() const
    {
        return view::highlighted_index(left_page, filtered, selected_left);
    }

    bool UICashShop::avatar_preview_ready() const
    {
        return preview_look.get_body() && preview_look.get_hair() &&
            preview_look.get_face();
    }

    void UICashShop::refresh_preview()
    {
        // The preview avatar is always the player look plus the accumulated
        // try-on set; same-slot picks override earlier ones until the reset
        // button clears the set back to the plain player look.
        preview_look = Stage::get().get_player().get_look();
        for (int32_t item_id : tryon_items)
            preview_look.add_equip(item_id, Texture::LoadMode::DEFERRED);
    }

    void UICashShop::add_tryon(size_t filtered_index)
    {
        if (filtered_index >= filtered.size())
            return;
        const int32_t item_id =
            model->catalog()[filtered[filtered_index]].item_id;
        if (!view::is_previewable_equip(item_id))
            return;
        const Equipslot::Id slot = EquipData::get(item_id).get_eqslot();
        const auto same_slot = [&](int32_t existing)
        {
            return EquipData::get(existing).get_eqslot() == slot;
        };
        tryon_items.erase(
            std::remove_if(tryon_items.begin(), tryon_items.end(), same_slot),
            tryon_items.end());
        tryon_items.push_back(item_id);
    }

    void UICashShop::clear_tryon()
    {
        if (tryon_items.empty())
            return;
        tryon_items.clear();
        refresh();
    }

    void UICashShop::refresh()
    {
        left_page = view::clamp_page(
            left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        right_page = view::clamp_page(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
            category_labels[entry].change_color(
                entry == category ? Text::BLACK : Text::DARKGREY);
        const bool equip_active = category == view::EQUIP_CATEGORY;
        for (int8_t sub = 0; sub < view::EQSUB_COUNT; ++sub)
            equip_sub_labels[sub].change_color(
                equip_active && sub == equip_sub ? Text::BLACK :
                    Text::DARKGREY);
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            currency_labels[choice].change_color(
                currency == currency_choice(choice) ? Text::WHITE :
                    Text::LIGHTGREY);
        const bool locker_active = right_pane == view::RightPane::LOCKER;
        tab_labels[0].change_color(
            locker_active ? Text::WHITE : Text::LIGHTGREY);
        tab_labels[1].change_color(
            locker_active ? Text::LIGHTGREY : Text::WHITE);
        transfer_hint.change_text(locker_active ?
            "双击物品取出" : "双击物品存入");
        page_labels[0].change_text(std::to_string(left_page + 1) + "/" +
            std::to_string(view::page_count(
                filtered.size(), view::CATALOG_PAGE_SIZE)));
        page_labels[1].change_text(std::to_string(right_page + 1) + "/" +
            std::to_string(view::page_count(
                right_count(), view::RIGHT_PAGE_SIZE)));
        std::string info = view::category_name(category);
        if (equip_active && equip_sub != 0)
            info += std::string(" · ") + view::equip_sub_name(equip_sub);
        catalog_info.change_text(info + " · " +
            std::to_string(filtered.size()) + " 件");
        pane_title.change_text(view::pane_name(right_pane));
        pane_info.change_text("共 " + std::to_string(right_count()) + " 件");
        refresh_rows();
    }

    void UICashShop::refresh_rows()
    {
        const auto left_range = view::page_range(
            left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        const auto right_range = view::page_range(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        preview_dirty = true;
        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
        {
            const size_t index = left_range.first + card;
            const bool active = index < left_range.second;
            buttons[BT_CARD_FIRST + card]->set_active(!pending && active);
            std::string name, meta;
            if (active)
            {
                const CashCommodity& item =
                    model->catalog()[filtered[index]];
                name = item.name;
                if (item.count > 1)
                    name += " x" + std::to_string(item.count);
                meta = period_label(item.period);
            }
            card_names[card].change_text(name);
            card_meta[card].change_text(meta);
        }
        for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
        {
            const size_t index = right_range.first + slot;
            const bool active = index < right_range.second;
            buttons[BT_SLOT_FIRST + slot]->set_active(!pending && active);
            const int16_t count = active ? right_item_count(index) : 0;
            slot_counts[slot].change_text(
                count > 1 ? std::to_string(count) : "");
        }
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            buttons[BT_TAB_FIRST + entry]->set_active(!pending);
            const size_t count = view::filter_catalog(model->catalog(),
                { female, entry, search.get_text() }).size();
            category_counts[entry].change_text(std::to_string(count));
            category_counts[entry].change_color(
                entry == category ? Text::BLACK : Text::DARKGREY);
        }
        const bool equip_menu_active = !pending && equip_menu_open &&
            category == view::EQUIP_CATEGORY;
        buttons[BT_EQSUB_CLOSE]->set_active(equip_menu_active);
        for (int8_t sub = 0; sub < view::EQSUB_COUNT; ++sub)
        {
            buttons[BT_EQSUB_FIRST + sub]->set_active(equip_menu_active);
            equip_sub_counts[sub].change_text(std::to_string(
                view::filter_catalog(model->catalog(),
                    { female, view::EQUIP_CATEGORY, search.get_text(), sub })
                    .size()));
        }
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            buttons[BT_CURRENCY_FIRST + choice]->set_active(!pending);
        const bool buy_visible = selected_left &&
            *selected_left / view::CATALOG_PAGE_SIZE == left_page;
        buttons[BT_BUY]->set_active(!pending && buy_visible &&
            !(equip_menu_open && category == view::EQUIP_CATEGORY));
        buttons[BT_RESET]->set_active(!tryon_items.empty());
        buttons[BT_FEATURED]->set_active(!pending && !filtered.empty());
        position_buy_button();
        const bool details_visible = selected_right &&
            *selected_right / view::RIGHT_PAGE_SIZE == right_page &&
            *selected_right < right_count();
        transfer_label.change_text(
            right_pane == view::RightPane::LOCKER ? "取出" : "存入");
        buy_label.change_color(
            buy_visible && !pending ? Text::BLACK : Text::LIGHTGREY);
        transfer_label.change_color(
            details_visible && !pending ? Text::WHITE : Text::LIGHTGREY);
        charge_label.change_color(pending ? Text::LIGHTGREY : Text::BLACK);
        exit_label.change_color(pending ? Text::LIGHTGREY : Text::WHITE);
        buttons[BT_TRANSFER]->set_active(!pending && details_visible);
        buttons[BT_CHARGE]->set_active(!pending);
        buttons[BT_EXIT]->set_active(!pending);
        buttons[BT_LEFT_PREV]->set_active(!pending && left_page > 0);
        buttons[BT_LEFT_NEXT]->set_active(!pending &&
            left_page + 1 < view::page_count(filtered.size(),
                view::CATALOG_PAGE_SIZE));
        buttons[BT_RIGHT_PREV]->set_active(!pending && right_page > 0);
        buttons[BT_RIGHT_NEXT]->set_active(!pending &&
            right_page + 1 < view::page_count(right_count(),
                view::RIGHT_PAGE_SIZE));
        const auto highlighted = highlighted_index();
        if (!highlighted)
        {
            featured_title.change_text("本期推荐");
            featured_name.change_text("");
            featured_meta.change_text("");
            featured_hint.change_text("当前筛选没有在售商品");
        }
        else
        {
            const CashCommodity& featured = model->catalog()[filtered[*highlighted]];
            const bool wearable = view::is_previewable_equip(featured.item_id);
            const bool selected = selected_left && *selected_left == *highlighted;
            featured_title.change_text(
                wearable ? (selected ? "试穿预览" : "本期推荐") :
                (selected ? "已选物品" : "本期推荐"));
            std::string name = featured.name;
            if (featured.count > 1)
                name += " x" + std::to_string(featured.count);
            featured_name.change_text(name);
            featured_meta.change_text(
                std::string(view::category_name(featured.category)) + " · " +
                period_label(featured.period));
            if (wearable)
                featured_hint.change_text(avatar_preview_ready() ?
                    (selected ? "已穿戴 · 点击其他装备试穿" :
                        "点击卡片选中并试穿") : "角色预览不可用");
            else
                featured_hint.change_text(selected ?
                    "该商品暂不可试穿 · 仅展示图标" : "点击卡片选中查看");
        }
        details_hint.change_text(details_visible ? "" : "选择物品查看详情");
        std::string detail_name, detail_count, detail_meta, detail_extra;
        if (details_visible)
        {
            if (const ItemData& data = ItemData::get(
                    right_item_id(*selected_right)))
                detail_name = data.get_name();
            detail_count = "数量 " +
                std::to_string(right_item_count(*selected_right));
            const CashLockerItem* locker = nullptr;
            if (right_pane == view::RightPane::LOCKER &&
                *selected_right < model->locker().size())
                locker = &model->locker()[*selected_right];
            const int32_t sn = locker ? locker->sn : 0;
            detail_meta = sn > 0 ? "SN " + std::to_string(sn) : "";
            const CashCommodity* commodity = find_commodity(model->catalog(), sn);
            if (locker && !locker->gift_from.empty())
                detail_extra = "赠送 " + locker->gift_from;
            else if (commodity)
                detail_extra = period_label(commodity->period) + " · " +
                    gender_name(commodity->gender);
            else if (locker && locker->expiration > 0)
                detail_extra = "有期限";
        }
        details_name.change_text(detail_name);
        details_count.change_text(detail_count);
        details_meta.change_text(detail_meta);
        details_extra.change_text(detail_extra);
        const bool locker_tab = right_pane == view::RightPane::LOCKER;
        buttons[BT_RIGHT_LOCKER_TAB]->set_active(!pending && !locker_tab);
        buttons[BT_RIGHT_INVENTORY_TAB]->set_active(!pending && locker_tab);
    }

    void UICashShop::change_right_pane(view::RightPane pane)
    {
        right_pane = pane;
        right_page = 0;
        selected_right.reset();
        refresh();
    }

    void UICashShop::set_pending(bool value)
    {
        pending = value;
        refresh();
    }

    void UICashShop::select_new_locker_item(int64_t cash_id)
    {
        right_pane = view::RightPane::LOCKER;
        const auto& items = model->locker();
        auto found = std::find_if(items.begin(), items.end(), [cash_id](const auto& item) {
            return item.cash_id == cash_id;
        });
        selected_right = found == items.end() ? std::optional<size_t>{} :
            std::optional<size_t>{static_cast<size_t>(found - items.begin())};
        right_page = selected_right ?
            *selected_right / view::RIGHT_PAGE_SIZE : 0;
        refresh();
    }

    void UICashShop::select_inventory_item(int64_t cash_id)
    {
        right_pane = view::RightPane::INVENTORY;
        const auto items = Stage::get().get_player().get_inventory().get_cash_items();
        auto found = std::find_if(items.begin(), items.end(), [cash_id](const auto& item) {
            return item.cash_id == cash_id;
        });
        selected_right = found == items.end() ? std::optional<size_t>{} :
            std::optional<size_t>{static_cast<size_t>(found - items.begin())};
        right_page = selected_right ?
            *selected_right / view::RIGHT_PAGE_SIZE : 0;
        refresh();
    }

    void UICashShop::request_purchase(size_t filtered_index)
    {
        if (filtered_index >= filtered.size())
            return;
        const CashCommodity item = model->catalog()[filtered[filtered_index]];
        const std::string question = "使用 " + std::string(view::currency_name(currency)) +
            " 购买 " + item.name + "，价格 " + view::format_amount(item.price) + "？";
        UI::get().emplace<UIYesNo>(question, [this, item](bool confirmed) {
            if (!confirmed)
                return;
            if (model->balances().get(currency) < item.price)
            {
                show_error("所选支付余额不足。");
                return;
            }
            set_pending(true);
            BuyCashItemPacket(item.sn, currency, item.package).dispatch();
        });
    }

    void UICashShop::request_transfer(size_t item_index)
    {
        if (pending || item_index >= right_count())
            return;
        pending_cash_id = right_cash_id(item_index);
        set_pending(true);
        if (right_pane == view::RightPane::LOCKER)
            TakeCashLockerItemPacket(pending_cash_id).dispatch();
        else
        {
            const auto items = Stage::get().get_player().get_inventory().get_cash_items();
            PutCashLockerItemPacket(
                pending_cash_id, static_cast<int8_t>(items[item_index].type)).dispatch();
        }
    }

    size_t UICashShop::right_count() const
    {
        return right_pane == view::RightPane::LOCKER ? model->locker().size() :
            Stage::get().get_player().get_inventory().get_cash_items().size();
    }

    int32_t UICashShop::right_item_id(size_t index) const
    {
        if (right_pane == view::RightPane::LOCKER)
            return model->locker()[index].item_id;
        return Stage::get().get_player().get_inventory().get_cash_items()[index].item_id;
    }

    int64_t UICashShop::right_cash_id(size_t index) const
    {
        if (right_pane == view::RightPane::LOCKER)
            return model->locker()[index].cash_id;
        return Stage::get().get_player().get_inventory().get_cash_items()[index].cash_id;
    }

    int16_t UICashShop::right_item_count(size_t index) const
    {
        if (right_pane == view::RightPane::LOCKER)
            return model->locker()[index].count;
        return Stage::get().get_player().get_inventory().get_cash_items()[index].count;
    }

    int32_t UICashShop::locker_count() const
    {
        return static_cast<int32_t>(model->locker().size());
    }

    int32_t UICashShop::inventory_count() const
    {
        return static_cast<int32_t>(
            Stage::get().get_player().get_inventory().get_cash_items().size());
    }

    int32_t UICashShop::nx_credit() const
    {
        return model->balances().nx_credit;
    }

    int32_t UICashShop::selected_right_row() const
    {
        return selected_right ?
            static_cast<int32_t>(*selected_right % view::RIGHT_PAGE_SIZE) : -1;
    }

    int32_t UICashShop::filtered_count() const
    {
        return static_cast<int32_t>(filtered.size());
    }

    int8_t UICashShop::equip_sub_index() const
    {
        return category == view::EQUIP_CATEGORY ? equip_sub : 0;
    }
    int32_t UICashShop::highlighted_item_id() const
    {
        const auto highlighted = highlighted_index();
        if (!highlighted)
            return 0;
        return model->catalog()[filtered[*highlighted]].item_id;
    }

    int32_t UICashShop::selected_left_index() const
    {
        return selected_left ? static_cast<int32_t>(*selected_left) : -1;
    }

    int32_t UICashShop::tryon_count() const
    {
        return static_cast<int32_t>(tryon_items.size());
    }

    int32_t UICashShop::preview_cap_id() const
    {
        return preview_look.get_equips().get_equip(Equipslot::CAP);
    }


    bool UICashShop::is_equip_menu_open() const
    {
        return equip_menu_open && category == view::EQUIP_CATEGORY;
    }

    int64_t UICashShop::transfer_cash_id() const
    {
        return pending_cash_id;
    }

    bool UICashShop::is_pending() const
    {
        return pending;
    }
}
