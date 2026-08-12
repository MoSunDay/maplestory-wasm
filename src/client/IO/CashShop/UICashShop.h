#pragma once

#include "CashShopModel.h"
#include "../UIElement.h"
#include "../Components/Textfield.h"
#include "../../Graphics/Text.h"

#include <memory>
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

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum class Mode : uint8_t { CATALOG, LOCKER, INVENTORY };
        enum Buttons : uint16_t
        {
            BT_EXIT,
            BT_CREDIT,
            BT_POINTS,
            BT_PREPAID,
            BT_CATALOG,
            BT_LOCKER,
            BT_INVENTORY,
            BT_CATEGORY_ALL,
            BT_CATEGORY_EQUIP,
            BT_CATEGORY_USE,
            BT_CATEGORY_SETUP,
            BT_CATEGORY_ETC,
            BT_CATEGORY_CASH,
            BT_CATEGORY_PACKAGE,
            BT_PREVIOUS,
            BT_NEXT,
            BT_ITEM_FIRST
        };

        static constexpr size_t ITEMS_PER_PAGE = 14;

        void rebuild();
        void refresh_labels();
        void set_mode(Mode value);
        void set_category(int8_t value);
        void request_purchase(size_t visible_index);
        void request_transfer(size_t visible_index);
        void set_pending(bool value);
        size_t result_count() const;
        std::string visible_name(size_t index) const;

        std::shared_ptr<CashShopModel> model;
        bool female;
        Mode mode;
        CashCurrency currency;
        int8_t category;
        size_t page;
        bool pending;
        int64_t pending_cash_id;
        std::vector<size_t> filtered;
        Textfield search;
        Text title;
        Text balance_label;
        Text status_label;
        Text item_names[ITEMS_PER_PAGE];
        Text item_prices[ITEMS_PER_PAGE];
    };
}
