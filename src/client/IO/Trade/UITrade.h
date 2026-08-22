#pragma once

#include "../UIDragElement.h"
#include "TradeState.h"

#include "../../Character/Inventory/Inventory.h"
#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Text.h"
#include "../Components/Textfield.h"

#include <array>
#include <memory>

namespace jrc
{
    class UITrade : public UIDragElement<PosTRADE>
    {
    public:
        static constexpr Type TYPE = TRADE;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UITrade(const Inventory& inventory, uint8_t local_slot);

        void draw(float alpha) const override;
        void update() override;
        void send_icon(const Icon& icon, Point<int16_t> cursor) override;
        void send_scroll(double yoffset) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursor) override;

        void set_participant(uint8_t slot, const std::string& name, const LookEntry& look);
        void set_item(bool partner, uint8_t slot, int32_t item_id, int16_t quantity);
        void set_meso(bool partner, int32_t amount);
        void add_chat(const std::string& message);
        void partner_confirmed();
        void offer_inventory_item(InventoryType::Id type, int16_t source, int16_t count);
        bool both_confirmed() const;
        void close();

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        void submit_item(InventoryType::Id type, int16_t source, int16_t quantity);
        void send_chat();
        void cancel_trade();
        void draw_offers(bool partner) const;

        enum Buttons { BT_CANCEL, BT_COIN, BT_ENTER, BT_RESET, BT_TRADE };

        const Inventory& inventory;
        uint8_t local_slot;
        uint8_t page;
        TradeState state;
        Textfield chatfield;
        std::array<std::unique_ptr<CharLook>, 2> looks;
        std::array<Text, 2> names;
        Text page_label;
    };
}
