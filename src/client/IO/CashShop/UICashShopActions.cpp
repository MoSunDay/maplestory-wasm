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
        right_page = view::clamp_page(right_page, right_count());
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
        left_page = view::clamp_page(left_page, filtered.size());
        right_page = view::clamp_page(right_page, right_count());
        category_label.change_text(std::string("分类: ") + view::category_name(category) + " >");
        currency_label.change_text(std::string("支付: ") + view::currency_name(currency) + " >");
        const CashBalances& balance = model->balances();
        balance_label.change_text("NX " + view::format_amount(balance.nx_credit) +
            "  点 " + view::format_amount(balance.maple_points) +
            "  预 " + view::format_amount(balance.nx_prepaid));
        right_mode_label.change_text(std::string(view::pane_name(right_pane)) + "  >");
        transfer_label.change_text(view::transfer_name(right_pane));
        left_page_label.change_text("<  " + std::to_string(left_page + 1) + "/" +
            std::to_string(view::page_count(filtered.size())) + "  >");
        right_page_label.change_text("<  " + std::to_string(right_page + 1) + "/" +
            std::to_string(view::page_count(right_count())) + "  >");
        refresh_rows();
    }

    void UICashShop::refresh_rows()
    {
        const auto left_range = view::page_range(left_page, filtered.size());
        const auto right_range = view::page_range(right_page, right_count());
        for (size_t row = 0; row < view::ROWS_PER_PANE; ++row)
        {
            const size_t left_index = left_range.first + row;
            const size_t right_index = right_range.first + row;
            const bool left_active = left_index < left_range.second;
            const bool right_active = right_index < right_range.second;
            buttons[BT_LEFT_FIRST + row]->set_active(!pending && left_active);
            buttons[BT_RIGHT_FIRST + row]->set_active(!pending && right_active);
            left_names[row].change_text(left_active ?
                model->catalog()[filtered[left_index]].name : "");
            left_details[row].change_text(left_active ?
                view::format_amount(model->catalog()[filtered[left_index]].price) + " NX" : "");
            right_names[row].change_text(right_active ?
                CashShopModel::item_name(right_item_id(right_index)) : "");
            right_details[row].change_text(right_active ?
                (right_pane == view::RightPane::LOCKER ? "再次点击取出" : "再次点击存入") : "");
        }
        buttons[BT_BUY]->set_active(!pending && selected_left.has_value());
        buttons[BT_BUY_HIT]->set_active(!pending && selected_left.has_value());
        buttons[BT_TRANSFER]->set_active(!pending && selected_right.has_value());
        buttons[BT_SELECTED_TRANSFER]->set_active(!pending && selected_right.has_value());
        if (selected_right)
        {
            buttons[BT_SELECTED_TRANSFER]->set_position(
                row_position(true, *selected_right % view::ROWS_PER_PANE));
        }
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
        right_page = selected_right ? *selected_right / view::ROWS_PER_PANE : 0;
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
        right_page = selected_right ? *selected_right / view::ROWS_PER_PANE : 0;
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

    Point<int16_t> UICashShop::row_position(bool right, size_t row) const
    {
        return { static_cast<int16_t>(right ? 246 : 12),
            static_cast<int16_t>(116 + row * 42) };
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
        return selected_right ? static_cast<int32_t>(*selected_right % view::ROWS_PER_PANE) : -1;
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
