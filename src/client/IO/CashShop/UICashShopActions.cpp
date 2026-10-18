#include "UICashShop.h"

#include "../UI.h"
#include "../UITypes/UINotice.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"

#include <algorithm>

namespace jrc
{
    namespace view = cash_shop_view;

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
        filtered = view::filter_catalog(model->catalog(), { female, category, search.get_text() });
        left_page = 0;
        selected_left.reset();
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
                entry == category ? Text::YELLOW : Text::LIGHTGREY);
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            currency_labels[choice].change_color(
                currency == currency_choice(choice) ? Text::YELLOW :
                    Text::LIGHTGREY);
        const bool locker_active = right_pane == view::RightPane::LOCKER;
        tab_labels[0].change_color(locker_active ? Text::BLACK : Text::WHITE);
        tab_labels[1].change_color(locker_active ? Text::WHITE : Text::BLACK);
        transfer_hint.change_text(locker_active ?
            "再次点击物品可快速取出" : "再次点击物品可快速存入");
        page_labels[0].change_text(std::to_string(left_page + 1) + "/" +
            std::to_string(view::page_count(
                filtered.size(), view::CATALOG_PAGE_SIZE)));
        page_labels[1].change_text(std::to_string(right_page + 1) + "/" +
            std::to_string(view::page_count(
                right_count(), view::RIGHT_PAGE_SIZE)));
        refresh_rows();
    }

    void UICashShop::refresh_rows()
    {
        const auto left_range = view::page_range(
            left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        const auto right_range = view::page_range(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
        {
            const size_t index = left_range.first + card;
            const bool active = index < left_range.second;
            buttons[BT_CARD_FIRST + card]->set_active(!pending && active);
            std::string name;
            if (active)
            {
                const CashCommodity& item =
                    model->catalog()[filtered[index]];
                name = item.name;
                if (item.count > 1)
                    name += " x" + std::to_string(item.count);
            }
            card_names[card].change_text(name);
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
            buttons[BT_TAB_FIRST + entry]->set_active(!pending);
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            buttons[BT_CURRENCY_FIRST + choice]->set_active(!pending);
        buttons[BT_BUY]->set_active(!pending && selected_left.has_value());
        position_buy_button();
        buttons[BT_TRANSFER]->set_active(!pending && selected_right.has_value());
        buttons[BT_CHARGE]->set_active(!pending);
        buttons[BT_EXIT]->set_active(!pending);
        buttons[BT_LEFT_PREV]->set_active(!pending);
        buttons[BT_LEFT_NEXT]->set_active(!pending);
        buttons[BT_RIGHT_PREV]->set_active(!pending);
        buttons[BT_RIGHT_NEXT]->set_active(!pending);
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
        if (item_index >= right_count())
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

    int64_t UICashShop::transfer_cash_id() const
    {
        return pending_cash_id;
    }

    bool UICashShop::is_pending() const
    {
        return pending;
    }
}
