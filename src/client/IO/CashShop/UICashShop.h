#pragma once

#include "CashShopModel.h"
#include "CashShopView.h"
#include "../Components/Textfield.h"
#include "../UIElement.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

#include <memory>
#include <optional>
#include <vector>

namespace jrc
{
    class UICashShop : public UIElement
    {
    public:
        static constexpr Type TYPE = CASHSHOP;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UICashShop(std::shared_ptr<CashShopModel> model, bool female);

        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t width, int16_t height) override;
        CursorResult send_cursor(bool pressed, Point<int16_t> cursorpos) override;
        void send_scroll(double yoffset) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

        void set_balances(CashBalances balances);
        void replace_locker(std::vector<CashLockerItem> items);
        void purchase_succeeded(CashLockerItem item);
        void package_purchase_succeeded(std::vector<CashLockerItem> items);
        void take_succeeded();
        void put_succeeded(CashLockerItem item);
        void show_error(const std::string& message);
        void reconnect_failed(const std::string& address);

        int32_t locker_count() const;
        int32_t inventory_count() const;
        int32_t nx_credit() const;
        int32_t selected_right_row() const;
        int64_t transfer_cash_id() const;
        bool is_pending() const;

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum Buttons : uint16_t
        {
            BT_BUY_HIT,
            BT_SELECTED_TRANSFER,
            BT_EXIT,
            BT_BUY,
            BT_TRANSFER,
            BT_CATEGORY,
            BT_CURRENCY,
            BT_RIGHT_MODE,
            BT_LEFT_PREVIOUS,
            BT_LEFT_NEXT,
            BT_RIGHT_PREVIOUS,
            BT_RIGHT_NEXT,
            BT_LEFT_FIRST,
            BT_RIGHT_FIRST = BT_LEFT_FIRST + cash_shop_view::ROWS_PER_PANE
        };

        void rebuild_catalog();
        void refresh();
        void refresh_rows();
        void change_right_pane(cash_shop_view::RightPane pane);
        void set_pending(bool value);
        void select_new_locker_item(int64_t cash_id);
        void select_inventory_item(int64_t cash_id);
        void request_purchase(size_t filtered_index);
        void request_transfer(size_t item_index);
        size_t right_count() const;
        int32_t right_item_id(size_t index) const;
        int64_t right_cash_id(size_t index) const;
        Point<int16_t> row_position(bool right, size_t row) const;

        std::shared_ptr<CashShopModel> model;
        bool female;
        cash_shop_view::RightPane right_pane;
        CashCurrency currency;
        int8_t category;
        size_t left_page;
        size_t right_page;
        bool pending;
        int64_t pending_cash_id;
        std::optional<size_t> selected_left;
        std::optional<size_t> selected_right;
        std::vector<size_t> filtered;
        Point<int16_t> last_cursor_position;

        Texture selection;
        Textfield search;
        Text title;
        Text category_label;
        Text currency_label;
        Text search_label;
        Text balance_label;
        Text right_mode_label;
        Text transfer_label;
        Text left_page_label;
        Text right_page_label;
        Text left_names[cash_shop_view::ROWS_PER_PANE];
        Text left_details[cash_shop_view::ROWS_PER_PANE];
        Text right_names[cash_shop_view::ROWS_PER_PANE];
        Text right_details[cash_shop_view::ROWS_PER_PANE];
    };
}
