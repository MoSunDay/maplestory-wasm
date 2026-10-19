#pragma once

#include "CashShopLayout.h"
#include "CashShopModel.h"
#include "CashShopSkin.h"
#include "CashShopView.h"
#include "../Components/Textfield.h"
#include "../UIElement.h"
#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

#include <memory>
#include <optional>
#include <vector>

namespace jrc
{
    // Full-screen cash shop window painted from the modern CashShop.img set.
    // The background bitmap is stretched from its native 1024x768 to fill the
    // current viewport, while the 800x600 design canvas (artwork, labels and
    // hit areas on one 25/32 pixel grid) is centred via the shared ui_layout
    // origin. The element is positioned at that origin so button hit tests
    // keep using design-space coordinates.
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
        int32_t filtered_count() const;
        int8_t equip_sub_index() const;
        bool is_equip_menu_open() const;
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
            BT_EQSUB_CLOSE,
            BT_EQSUB_FIRST,
            BT_EQSUB_LAST = BT_EQSUB_FIRST + cash_shop_view::EQSUB_COUNT,
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
            BT_RIGHT_NEXT,
            BT_FEATURED
        };

        void rebuild_catalog();
        void refresh();
        void refresh_rows();
        void press_equip_menu(uint16_t id);
        std::optional<size_t> highlighted_index() const;
        void refresh_preview();
        bool avatar_preview_ready() const;
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
        void draw_action(uint16_t id,
            const cash_shop_skin::CashShopButtonSkin& source,
            Point<int16_t> pos, Point<int16_t> size) const;
        void draw_icon(const Texture& icon, Point<int16_t> center,
            int16_t max_side) const;
        void draw_balance_icons() const;
        void draw_catalog_info() const;
        void draw_category_counts() const;
        void draw_equip_menu() const;
        void draw_card_meta() const;
        void draw_details_text() const;
        void draw_feature_panel() const;
        void draw_sidebar_legend() const;
        void draw_catalog_hint() const;
        void position_buy_button();

        std::shared_ptr<CashShopModel> model;
        bool female;
        cash_shop_view::RightPane right_pane;
        CashCurrency currency;
        int8_t category;
        int8_t equip_sub;
        bool equip_menu_open;
        size_t left_page;
        size_t right_page;
        bool pending;
        bool balance_requested;
        bool preview_dirty;
        int64_t pending_cash_id;
        std::optional<size_t> selected_left;
        std::optional<size_t> selected_right;
        std::vector<size_t> filtered;
        Point<int16_t> last_cursor_position;

        cash_shop_skin::CashShopSkin skin;
        CharLook preview_look;

        Textfield search;
        Text title;
        Text category_title;
        Text equip_menu_title;
        Text equip_close_label;
        Text pane_title;
        Text pane_info;
        Text search_label;
        Text transfer_hint;
        Text details_hint;
        Text details_name;
        Text details_count;
        Text buy_label;
        Text transfer_label;
        Text charge_label;
        Text exit_label;
        Text tab_labels[2];
        Text balance_labels[CURRENCY_CHOICES];
        Text currency_labels[CURRENCY_CHOICES];
        Text category_labels[cash_shop_view::CATEGORY_COUNT];
        Text card_names[cash_shop_view::CATALOG_PAGE_SIZE];
        Text slot_counts[cash_shop_view::RIGHT_PAGE_SIZE];
        Text page_labels[2];
        Text catalog_info;
        Text catalog_hint;
        Text legend_title;
        Text legend_note;
        Text legend_labels[5];
        Text category_counts[cash_shop_view::CATEGORY_COUNT];
        Text equip_sub_labels[cash_shop_view::EQSUB_COUNT];
        Text equip_sub_counts[cash_shop_view::EQSUB_COUNT];
        Text card_meta[cash_shop_view::CATALOG_PAGE_SIZE];
        Text details_meta;
        Text details_extra;
        Text featured_title;
        Text featured_name;
        Text featured_meta;
        Text featured_hint;
    };
}
