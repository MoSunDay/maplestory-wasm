#pragma once

#include "CashShopLayout.h"
#include "CashShopModel.h"
#include "CashShopSkin.h"
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
    // Full-screen cash shop window painted from the modern CashShop.img set.
    // All child geometry lives in the 800x600 design space and is mapped to
    // the real viewport through cash_shop_layout; the element itself stays
    // anchored at (0, 0) covering the whole screen.
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
        static constexpr size_t CURRENCY_CHOICES = 3;

        enum Buttons : uint16_t
        {
            BT_EXIT,
            BT_CHARGE,
            BT_BUY,
            BT_CARD_FIRST,
            BT_CARD_LAST = BT_CARD_FIRST + cash_shop_view::CATALOG_PAGE_SIZE,
            BT_SLOT_FIRST,
            BT_SLOT_LAST = BT_SLOT_FIRST + cash_shop_view::RIGHT_PAGE_SIZE,
            BT_TAB_FIRST,
            BT_TAB_LAST = BT_TAB_FIRST + cash_shop_view::CATEGORY_COUNT,
            BT_CURRENCY_FIRST,
            BT_CURRENCY_LAST = BT_CURRENCY_FIRST + CURRENCY_CHOICES,
            BT_RIGHT_LOCKER_TAB,
            BT_RIGHT_INVENTORY_TAB,
            BT_TRANSFER,
            BT_LEFT_PREV,
            BT_LEFT_NEXT,
            BT_RIGHT_PREV,
            BT_RIGHT_NEXT
        };

        void rebuild_catalog();
        void refresh();
        void refresh_rows();
        static CashCurrency currency_choice(size_t index);
        static const char* currency_short_name(CashCurrency value);
        void change_right_pane(cash_shop_view::RightPane pane);
        void set_pending(bool value);
        void select_new_locker_item(int64_t cash_id);
        void select_inventory_item(int64_t cash_id);
        void request_purchase(size_t filtered_index);
        void request_transfer(size_t item_index);
        size_t right_count() const;
        int32_t right_item_id(size_t index) const;
        int64_t right_cash_id(size_t index) const;
        int16_t right_item_count(size_t index) const;
        Point<int16_t> map_to_screen(Point<int16_t> design) const;
        void position_static_buttons();
        void position_buy_button();

        std::shared_ptr<CashShopModel> model;
        bool female;
        cash_shop_view::RightPane right_pane;
        CashCurrency currency;
        int8_t category;
        size_t left_page;
        size_t right_page;
        bool pending;
        bool balance_requested;
        int64_t pending_cash_id;
        std::optional<size_t> selected_left;
        std::optional<size_t> selected_right;
        std::vector<size_t> filtered;
        Point<int16_t> last_cursor_position;

        cash_shop_skin::CashShopSkin skin;
        float screen_scale;
        Point<int16_t> screen_offset;

        Textfield search;
        Text title;
        Text search_label;
        Text transfer_hint;
        Text tab_labels[2];
        Text balance_labels[CURRENCY_CHOICES];
        Text currency_labels[CURRENCY_CHOICES];
        Text category_labels[cash_shop_view::CATEGORY_COUNT];
        Text card_names[cash_shop_view::CATALOG_PAGE_SIZE];
        Text slot_counts[cash_shop_view::RIGHT_PAGE_SIZE];
        Text page_labels[2];
    };
}
