#pragma once

#include "CashShopLayout.h"
#include "CashShopModel.h"
#include "CashShopSettlement.h"
#include "CashShopSkin.h"
#include "CashShopView.h"
#include "UICashShopButtons.h"
#include "../Components/Textfield.h"
#include "../UIElement.h"
#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace jrc
{
    // The currency row length lives with the button geometry table.
    using cash_shop_buttons::CURRENCY_CHOICES;

    // Full-screen cash shop window painted from the modern CashShop.img set.
    // The background bitmap is stretched from its native 1024x768 to fill the
    // current viewport, while the 800x600 design canvas (artwork, labels and
    // hit areas on one 25/32 pixel grid) is scaled uniformly to fill taller
    // viewports (1.2x at 1280x720) and centred via the layout origin. The
    // element is positioned at that origin, and the hit areas live on the
    // scaled grid through cash_shop_buttons::apply_geometry, so button hit
    // tests keep using design-space coordinates.
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
        void doubleclick(Point<int16_t> cursorpos) override;

        void set_balances(CashBalances balances);
        void replace_locker(std::vector<CashLockerItem> items);
        void purchase_succeeded(CashLockerItem item);
        void package_purchase_succeeded(std::vector<CashLockerItem> items);
        void take_succeeded(int64_t cash_id);
        void put_succeeded(CashLockerItem item, int64_t cash_id);
        void show_error(const std::string& message);
        void reconnect_failed(const std::string& address);
        // SetField that ends a cash shop session: the pending EXIT was acked
        // (leave_succeeded) or the reply named another character, which means
        // the shop has to be entered again (leave_failed).
        void leave_succeeded();
        void leave_failed();

        int32_t locker_count() const;
        int32_t inventory_count() const;
        int32_t nx_credit() const;
        int32_t selected_right_row() const;
        int64_t selected_right_cash_id() const;
        int32_t filtered_count() const;
        int8_t equip_sub_index() const;
        int32_t highlighted_item_id() const;
        int32_t selected_left_index() const;
        int32_t preview_cap_id() const;
        int32_t preview_weapon_id() const;
        int32_t tryon_count() const;
        bool is_equip_menu_open() const;
        int64_t transfer_cash_id() const;
        bool is_pending() const;
        cash_shop_settlement::PendingKind pending_kind() const;
        // Purchases/transfers need loaded balances, no failure, no pending
        // operation and no required re-entry.
        bool balances_ready() const;
        bool balance_failed() const;
        bool can_mutate() const;
        bool can_exit() const;
        bool require_reenter() const;
        // True when the pending operation waits for this reply opcode; a reply
        // that does not match must not touch the shop state.
        bool expects(cash_shop_settlement::PendingKind response) const;

        Point<int16_t> button_center(uint16_t id) const
        {
            const auto found = buttons.find(id);
            if (found == buttons.end())
                return { -32768, -32768 };
            const Rectangle<int16_t> rect = found->second->bounds(position);
            return {
                static_cast<int16_t>((rect.l() + rect.r()) / 2),
                static_cast<int16_t>((rect.t() + rect.b()) / 2)
            };
        }

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        void rebuild_catalog();
        void clear_search();
        void refresh();
        void refresh_rows();
        void refresh_category_icons();
        void press_equip_menu(uint16_t id);
        std::optional<size_t> highlighted_index() const;
        void refresh_preview();
        void add_tryon(size_t filtered_index);
        void clear_tryon();
        bool avatar_preview_ready() const;
        static CashCurrency currency_choice(size_t index);
        static const char* currency_short_name(CashCurrency value);
        void change_right_pane(cash_shop_view::RightPane pane);
        // One outgoing operation at a time; the deadline covers a lost reply
        // instead of counting frames, so a stalled tab does not expire early.
        void begin_pending(cash_shop_settlement::PendingKind kind,
            int64_t cash_id);
        void finish_pending();
        void clear_pending();
        void select_new_locker_item(int64_t cash_id);
        void select_inventory_item(int64_t cash_id);
        void request_purchase(size_t filtered_index);
        // Selected catalog entry exists and lives on the currently shown page.
        bool left_selection_visible() const;
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
        void draw_right_pane() const;
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
        cash_shop_settlement::PendingKind pending_kind_;
        // showCash only pushes the balances once; balances_loaded_ is set by
        // that push (0x144), balance_failed_ by its deadline.
        bool balances_loaded_;
        bool balance_failed_;
        // Set when a pending operation timed out or a leave reply named another
        // character: the shop must be re-entered before anything can mutate.
        bool require_reenter_;
        // Lets a late balance push clear only the timeout text it owns.
        bool deferred_balance_error_;
        std::optional<cash_shop_settlement::Clock::time_point>
            balance_deadline_;
        std::optional<cash_shop_settlement::Clock::time_point>
            pending_deadline_;
        bool preview_dirty;
        int64_t pending_cash_id;
        std::optional<size_t> selected_left;
        std::optional<size_t> selected_right;
        std::vector<size_t> filtered;
        std::string applied_query;
        // Error raised from a modal callback (UIYesNo) is queued here and
        // emplaced from update(), because emplace<UIOk> resets the shared
        // NOTICE slot and would destroy the still-executing UIYesNo.
        std::string deferred_error;
        int32_t category_icon_ids[cash_shop_view::CATEGORY_COUNT] = {};
        Point<int16_t> last_cursor_position;

        cash_shop_skin::CashShopSkin skin;
        CharLook preview_look;
        std::vector<int32_t> tryon_items;

        Textfield search;
        Text title;
        Text category_title;
        Text equip_menu_title;
        Text equip_close_label;
        Text pane_title;
        Text pane_info;
        Text search_label;
        Text search_clear_label;
        Text rail_help;
        Text rail_status;
        Text details_title;
        Text empty_catalog_text;
        Text transfer_hint;
        Text details_hint;
        Text details_name;
        Text details_count;
        Text buy_label;
        Text reset_label;
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
