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
    namespace layout = cash_shop_layout;

    namespace
    {
        std::string period_label(int32_t period)
        {
            return period > 0 ? std::to_string(period) + "天" : "永久";
        }

        void truncate_text(Text& label, int16_t max_width)
        {
            if (max_width <= 0 || label.width() <= max_width)
                return;
            std::string text = label.get_text();
            while (!text.empty() && label.width() > max_width)
            {
                text.pop_back();
                label.change_text(text + "..");
            }
        }
    }

    void UICashShop::set_balances(CashBalances balances)
    {
        // 0x144 is balance data only: it must never unlock or settle an
        // operation, and a late push (after the operation settled or timed
        // out) just refreshes the balances. But paired with a still-pending
        // BUY/TAKE/PUT/SLOTS it is the server's rejection notice — the
        // rejection paths reply with exactly this push and no operation ack —
        // so fail fast instead of letting the operation time out. EXIT is
        // not affected: 0x144 does not fail an exit.
        model->set_balances(balances);
        balances_loaded_ = true;
        balance_failed_ = false;
        balance_deadline_.reset();
        if (deferred_balance_error_)
        {
            // The only text this push owns is the balance timeout hint.
            deferred_error.clear();
            deferred_balance_error_ = false;
        }
        if (cash_shop_settlement::is_operation(pending_kind_))
        {
            // Success paths always ack the operation first, so a pending
            // operation here means this push is the whole rejection reply.
            // show_error drops the pending state, queues the text and
            // refreshes through clear_pending().
            show_error("商城操作未完成，请重试。");
            return;
        }
        refresh();
    }

    bool UICashShop::balances_ready() const
    {
        return balances_loaded_ && !balance_failed_;
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
        // Capture the id before the move: finish_pending() resets the stored
        // transfer id, so the row selection needs the item's own cash id.
        const int64_t cash_id = item.cash_id;
        model->add_locker(std::move(item));
        finish_pending();
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
        finish_pending();
        select_new_locker_item(last_cash_id);
    }

    void UICashShop::take_succeeded(int64_t cash_id)
    {
        model->remove_locker(cash_id);
        finish_pending();
        select_inventory_item(cash_id);
    }

    void UICashShop::put_succeeded(CashLockerItem item, int64_t cash_id)
    {
        const int64_t locker_cash_id = item.cash_id;
        Stage::get().get_player().get_inventory().remove_cash_item(cash_id);
        model->add_locker(std::move(item));
        finish_pending();
        select_new_locker_item(locker_cash_id);
    }

    void UICashShop::doubleclick(Point<int16_t> cursorpos)
    {
        if (!can_mutate())
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
        // Server-side rejections (0x5C, unknown opcode), the client-side
        // balance check and the reconnect path all land here: the operation
        // is over, so drop the pending state before queueing the text.
        clear_pending();
        // Deferred: callers include the UIYesNo yes-handler, and an immediate
        // emplace<UIOk> would free that still-running element (both live in
        // the shared NOTICE slot). update() shows the text on the next tick.
        deferred_error = message;
        deferred_balance_error_ = false;
    }

    void UICashShop::leave_succeeded()
    {
        // clear_pending() also drops the stored cash id; the ack itself is
        // not an error and the shop stays usable when it is entered again.
        clear_pending();
        require_reenter_ = false;
        refresh();
    }

    void UICashShop::leave_failed()
    {
        require_reenter_ = true;
        UI::get().enable();
        show_error("返回游戏角色不匹配，请重试。");
    }

    void UICashShop::reconnect_failed(const std::string& address)
    {
        UI::get().enable();
        show_error("返回游戏服务器失败：" + address);
    }

    void UICashShop::rebuild_catalog()
    {
        filtered = view::filter_catalog(model->catalog(),
            { female, category, applied_query,
              category == view::EQUIP_CATEGORY ? equip_sub : 0 });
        left_page = 0;
        selected_left.reset();
        refresh();
    }

    void UICashShop::clear_search()
    {
        applied_query.clear();
        search.change_text("");
        rebuild_catalog();
    }

    void UICashShop::refresh_category_icons()
    {
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
            category_icon_ids[entry] = 0;

        // One representative icon per category. "All" keeps the drawn
        // four-square glyph; the remaining rows use the first catalog item
        // whose Item.nx entry actually loaded.
        for (const CashCommodity& item : model->catalog())
        {
            const int8_t entry = item.category;
            if (entry <= 0 || entry >= view::CATEGORY_COUNT ||
                category_icon_ids[entry] != 0 || item.item_id == 0)
                continue;
            if (ItemData::get(item.item_id).is_valid())
                category_icon_ids[entry] = item.item_id;
        }
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

    bool UICashShop::left_selection_visible() const
    {
        return selected_left && *selected_left < filtered.size() &&
            *selected_left / view::CATALOG_PAGE_SIZE == left_page;
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
        preview_look.prepare_movement_assets();
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
        {
            const bool selected = equip_active && sub == equip_sub;
            equip_sub_labels[sub].change_color(
                selected ? Text::BLACK : Text::DARKGREY);
            equip_sub_counts[sub].change_color(
                selected ? Text::BLACK : Text::DARKGREY);
        }
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
        if (filtered.empty())
            empty_catalog_text.change_text(applied_query.empty() ?
                "此分类暂无商品" : "未找到匹配商品\n按清除按钮恢复");
        else
            empty_catalog_text.change_text("");
        std::string status = view::equip_sub_name(equip_sub);
        status += "\n筛选 " + std::to_string(filtered.size()) + " 件";
        status += "\n\n点击分类切换\n返回浏览全部";
        rail_status.change_text(status);
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
            buttons[BT_CARD_FIRST + card]->set_active(!is_pending() && active);
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
            truncate_text(card_names[card], layout::CARD_NAME_WIDTH);
            card_meta[card].change_text(meta);
        }
        for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
        {
            const size_t index = right_range.first + slot;
            const bool active = index < right_range.second;
            buttons[BT_SLOT_FIRST + slot]->set_active(!is_pending() && active);
            const int16_t count = active ? right_item_count(index) : 0;
            slot_counts[slot].change_text(
                count > 1 ? std::to_string(count) : "");
        }
        const bool equip_menu_active = !is_pending() && equip_menu_open &&
            category == view::EQUIP_CATEGORY;
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            buttons[BT_TAB_FIRST + entry]->set_active(
                !is_pending() && !equip_menu_active);
            const size_t count = view::filter_catalog(model->catalog(),
                { female, entry, applied_query }).size();
            category_counts[entry].change_text(std::to_string(count));
            category_counts[entry].change_color(
                entry == category ? Text::BLACK : Text::DARKGREY);
        }
        buttons[BT_EQSUB_CLOSE]->set_active(equip_menu_active);
        for (int8_t sub = 0; sub < view::EQSUB_COUNT; ++sub)
        {
            buttons[BT_EQSUB_FIRST + sub]->set_active(equip_menu_active);
            equip_sub_counts[sub].change_text(std::to_string(
                view::filter_catalog(model->catalog(),
                    { female, view::EQUIP_CATEGORY, applied_query, sub })
                    .size()));
        }
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            buttons[BT_CURRENCY_FIRST + choice]->set_active(!is_pending());
        // A price-0 item must also wait for the balances to load, otherwise
        // the confirmation could pass the (not yet received) balance check.
        const bool buy_ready = left_selection_visible() && can_mutate();
        buttons[BT_SEARCH_CLEAR]->set_active(!is_pending() &&
            !applied_query.empty());
        buttons[BT_BUY]->set_active(buy_ready &&
            !(equip_menu_open && category == view::EQUIP_CATEGORY));
        buttons[BT_RESET]->set_active(!tryon_items.empty());
        buttons[BT_FEATURED]->set_active(!is_pending() && !filtered.empty());
        position_buy_button();
        // A right selection is only actionable while it is still on the
        // visible page; the details card that used this flag is gone, but
        // the transfer button keeps the same visibility rule.
        const bool selection_visible = selected_right &&
            *selected_right / view::RIGHT_PAGE_SIZE == right_page &&
            *selected_right < right_count();
        transfer_label.change_text(
            right_pane == view::RightPane::LOCKER ? "取出" : "存入");
        buy_label.change_color(
            can_mutate() ? Text::BLACK : Text::LIGHTGREY);
        transfer_label.change_color(
            selection_visible && can_mutate() ? Text::WHITE : Text::LIGHTGREY);
        charge_label.change_color(
            is_pending() ? Text::LIGHTGREY : Text::BLACK);
        exit_label.change_color(
            can_exit() ? Text::WHITE : Text::LIGHTGREY);
        buttons[BT_TRANSFER]->set_active(can_mutate() && selection_visible);
        buttons[BT_CHARGE]->set_active(!is_pending());
        buttons[BT_EXIT]->set_active(can_exit());
        buttons[BT_LEFT_PREV]->set_active(!is_pending() && left_page > 0);
        buttons[BT_LEFT_NEXT]->set_active(!is_pending() &&
            left_page + 1 < view::page_count(filtered.size(),
                view::CATALOG_PAGE_SIZE));
        buttons[BT_RIGHT_PREV]->set_active(!is_pending() && right_page > 0);
        buttons[BT_RIGHT_NEXT]->set_active(!is_pending() &&
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
        const bool locker_tab = right_pane == view::RightPane::LOCKER;
        buttons[BT_RIGHT_LOCKER_TAB]->set_active(!is_pending() && !locker_tab);
        buttons[BT_RIGHT_INVENTORY_TAB]->set_active(!is_pending() && locker_tab);
        refresh_slotbuy();
    }

    void UICashShop::change_right_pane(view::RightPane pane)
    {
        right_pane = pane;
        right_page = 0;
        selected_right.reset();
        refresh();
    }

    void UICashShop::begin_pending(cash_shop_settlement::PendingKind kind,
        int64_t cash_id)
    {
        pending_kind_ = kind;
        pending_cash_id = cash_id;
        // Budget + grace: a slow-but-live reply must not be turned into an
        // error by a single dropped frame or modal transition.
        pending_deadline_ = cash_shop_settlement::Clock::now() +
            cash_shop_settlement::timeout_for(kind) +
            cash_shop_settlement::SETTLE_GRACE;
        refresh();
    }

    void UICashShop::finish_pending()
    {
        // A matching success reply settles the operation; the selection move
        // happens in the caller with the cash id captured before the move.
        clear_pending();
    }

    void UICashShop::clear_pending()
    {
        pending_kind_ = cash_shop_settlement::PendingKind::NONE;
        pending_deadline_.reset();
        pending_cash_id = 0;
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
        // The gate covers loaded balances, a failed load, a pending operation
        // and a required re-entry; the page check rejects a stale selection.
        if (!can_mutate() || filtered_index >= filtered.size() ||
            filtered_index / view::CATALOG_PAGE_SIZE != left_page)
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
            begin_pending(cash_shop_settlement::PendingKind::BUY, 0);
            BuyCashItemPacket(item.sn, currency, item.package).dispatch();
        });
    }

    void UICashShop::request_transfer(size_t item_index)
    {
        if (!can_mutate() || item_index >= right_count())
            return;
        const bool taking = right_pane == view::RightPane::LOCKER;
        const int64_t cash_id = right_cash_id(item_index);
        begin_pending(taking ? cash_shop_settlement::PendingKind::TAKE :
            cash_shop_settlement::PendingKind::PUT, cash_id);
        if (taking)
            TakeCashLockerItemPacket(cash_id).dispatch();
        else
        {
            const auto items = Stage::get().get_player().get_inventory().get_cash_items();
            PutCashLockerItemPacket(
                cash_id, static_cast<int8_t>(items[item_index].type)).dispatch();
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

    int64_t UICashShop::selected_right_cash_id() const
    {
        if (!selected_right || *selected_right >= right_count())
            return 0;
        return right_cash_id(*selected_right);
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

    int32_t UICashShop::preview_weapon_id() const
    {
        return preview_look.get_equips().get_equip(Equipslot::WEAPON);
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
        return pending_kind_ != cash_shop_settlement::PendingKind::NONE;
    }

    cash_shop_settlement::PendingKind UICashShop::pending_kind() const
    {
        return pending_kind_;
    }

    bool UICashShop::balance_failed() const
    {
        return balance_failed_;
    }

    bool UICashShop::can_mutate() const
    {
        return cash_shop_settlement::can_mutate(
            balances_ready(), balance_failed_, is_pending(), require_reenter_);
    }

    bool UICashShop::can_exit() const
    {
        return cash_shop_settlement::can_exit(is_pending());
    }

    bool UICashShop::require_reenter() const
    {
        return require_reenter_;
    }

    bool UICashShop::expects(cash_shop_settlement::PendingKind response) const
    {
        return pending_kind_ == response &&
            cash_shop_settlement::is_operation(response);
    }
}
