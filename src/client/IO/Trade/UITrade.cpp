#include "UITrade.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../UITypes/UINotice.h"

#include "../../Data/ItemData.h"
#include "../../Net/Packets/TradePackets.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <limits>

namespace jrc
{
    UITrade::UITrade(const Inventory& inv, uint8_t slot)
        : UIDragElement<PosTRADE>({ 654, 24 }),
          inventory(inv), local_slot(slot), page(0),
          chatfield(Text::A11M, Text::LEFT, Text::DARKGREY,
              Rectangle<int16_t>(28, 570, 346, 369), 70),
          page_label(Text::A11M, Text::CENTER, Text::DARKGREY, "1-5 / 9")
    {
        nl::node source = nl::nx::ui["UIWindow2.img"]["CashTradingRoom"];
        sprites.emplace_back(source["backgrnd"]);
        sprites.emplace_back(source["backgrnd2"]);
        sprites.emplace_back(source["backgrnd3"]);
        buttons[BT_CANCEL] = std::make_unique<MapleButton>(source["BtClame"]);
        buttons[BT_COIN] = std::make_unique<MapleButton>(source["BtCoin"]);
        buttons[BT_ENTER] = std::make_unique<MapleButton>(source["BtEnter"]);
        buttons[BT_RESET] = std::make_unique<MapleButton>(source["BtReset"]);
        buttons[BT_TRADE] = std::make_unique<MapleButton>(source["BtTrade"]);
        buttons[BT_RESET]->set_active(false);
        buttons[BT_TRADE]->set_state(Button::DISABLED);

        names[0] = { Text::A12B, Text::CENTER, Text::DARKGREY, "等待中", 260 };
        names[1] = { Text::A12B, Text::CENTER, Text::DARKGREY, "等待对方", 260 };
        chatfield.set_enter_callback([this](const std::string&) { send_chat(); });
        dimension = { 654, 566 };
        active = true;
    }

    void UITrade::draw(float alpha) const
    {
        draw_sprites(alpha);
        names[0].draw(position + Point<int16_t>(163, 40));
        names[1].draw(position + Point<int16_t>(489, 40));
        if (looks[0])
            looks[0]->draw(position + Point<int16_t>(162, 108), false,
                Stance::STAND1, Expression::DEFAULT);
        if (looks[1])
            looks[1]->draw(position + Point<int16_t>(489, 108), true,
                Stance::STAND1, Expression::DEFAULT);

        draw_offers(false);
        draw_offers(true);
        page_label.draw(position + Point<int16_t>(327, 318));

        const std::string local_meso = "金币: " + std::to_string(state.meso(false));
        const std::string partner_meso = "金币: " + std::to_string(state.meso(true));
        Text(Text::A11B, Text::LEFT, Text::DARKGREY, local_meso)
            .draw(position + Point<int16_t>(31, 319));
        Text(Text::A11B, Text::LEFT, Text::DARKGREY, partner_meso)
            .draw(position + Point<int16_t>(356, 319));

        if (state.confirmed(false))
            Text(Text::A11B, Text::CENTER, Text::RED, "已确认", 120)
                .draw(position + Point<int16_t>(163, 73));
        if (state.confirmed(true))
            Text(Text::A11B, Text::CENTER, Text::RED, "对方已确认", 120)
                .draw(position + Point<int16_t>(489, 73));

        int16_t chat_y = 392;
        for (const std::string& message : state.chat())
        {
            Text(Text::A11M, Text::LEFT, Text::DARKGREY, message, 590)
                .draw(position + Point<int16_t>(30, chat_y));
            chat_y += 17;
        }
        chatfield.draw(position);
        draw_buttons(alpha);
    }

    void UITrade::draw_offers(bool partner) const
    {
        const int16_t x = partner ? 355 : 30;
        const uint8_t first = page == 0 ? 1 : 5;
        for (uint8_t row = 0; row < 5; ++row)
        {
            uint8_t slot = first + row;
            const TradeOffer& offer = state.item(partner, slot);
            if (offer.item_id == 0)
                continue;

            Point<int16_t> item_position = position + Point<int16_t>(x, 116 + row * 39);
            ItemData::get(offer.item_id).get_icon(false).draw(item_position);
            std::string label = ItemData::get(offer.item_id).get_name();
            if (offer.quantity > 1)
                label += " x" + std::to_string(offer.quantity);
            Text(Text::A11M, Text::LEFT, Text::DARKGREY, label, 240)
                .draw(item_position + Point<int16_t>(39, -25));
        }
    }

    void UITrade::update()
    {
        UIElement::update();
        chatfield.update(position);
    }

    void UITrade::set_participant(uint8_t slot, const std::string& name,
        const LookEntry& look)
    {
        bool partner = slot != local_slot;
        state.set_name(partner, name);
        names[partner ? 1 : 0].change_text(name);
        looks[partner ? 1 : 0] = std::make_unique<CharLook>(look);
        if (partner)
            buttons[BT_TRADE]->set_state(Button::NORMAL);
    }

    void UITrade::set_item(bool partner, uint8_t slot, int32_t item_id, int16_t quantity)
    {
        state.set_item(partner, slot, { item_id, quantity });
    }

    void UITrade::set_meso(bool partner, int32_t amount)
    {
        state.set_meso(partner, amount);
    }

    void UITrade::add_chat(const std::string& message)
    {
        state.add_chat(message);
    }

    void UITrade::partner_confirmed()
    {
        state.confirm(true);
        buttons[BT_COIN]->set_state(Button::DISABLED);
    }

    bool UITrade::both_confirmed() const
    {
        return state.confirmed(false) && state.confirmed(true);
    }

    void UITrade::close()
    {
        chatfield.set_state(Textfield::DISABLED);
        active = false;
    }

    void UITrade::offer_inventory_item(InventoryType::Id type, int16_t source,
        int16_t count)
    {
        if (state.confirmed(false) || state.confirmed(true))
        {
            UI::get().emplace<UIOk>("交易已确认，不能再更改物品。", []() {});
            return;
        }
        if (count < 1)
            return;
        if (state.next_local_slot() == 0)
        {
            UI::get().emplace<UIOk>("交易栏最多只能放入 9 件物品。", []() {});
            return;
        }
        if (count > 1)
        {
            UI::get().emplace<UIEnterNumber>("请输入交易数量。",
                [type, source](int32_t quantity) {
                    if (auto trade = UI::get().get_element<UITrade>())
                        trade->submit_item(type, source, static_cast<int16_t>(quantity));
                }, 1, count, 1, true);
            return;
        }
        submit_item(type, source, 1);
    }

    void UITrade::submit_item(InventoryType::Id type, int16_t source, int16_t quantity)
    {
        uint8_t target = state.next_local_slot();
        if (target == 0 || quantity < 1)
            return;
        TradeItemPacket(type, source, quantity, target).dispatch();
    }

    void UITrade::send_icon(const Icon& icon, Point<int16_t>)
    {
        icon.drop_on_trade();
    }

    void UITrade::send_scroll(double yoffset)
    {
        page = yoffset < 0 ? 1 : 0;
        page_label.change_text(page == 0 ? "1-5 / 9" : "5-9 / 9");
    }

    UIElement::CursorResult UITrade::send_cursor(bool clicked, Point<int16_t> cursor)
    {
        Cursor::State chat_state = chatfield.send_cursor(cursor, clicked);
        if (chat_state != Cursor::IDLE)
            return { chat_state, true };
        return UIDragElement::send_cursor(clicked, cursor);
    }

    void UITrade::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            cancel_trade();
    }

    void UITrade::send_chat()
    {
        if (!chatfield.empty())
        {
            TradeChatPacket(chatfield.get_text()).dispatch();
            chatfield.change_text("");
        }
    }

    void UITrade::cancel_trade()
    {
        TradeActionPacket(TradeActionPacket::EXIT).dispatch();
        close();
    }

    Button::State UITrade::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_CANCEL:
            cancel_trade();
            break;
        case BT_COIN:
            if (!state.confirmed(false) && !state.confirmed(true) && inventory.get_meso() > 0)
            {
                int32_t maximum = static_cast<int32_t>(std::min<int64_t>(
                    inventory.get_meso(), std::numeric_limits<int32_t>::max()));
                UI::get().emplace<UIEnterNumber>("请输入要追加的金币数量。",
                    [](int32_t amount) { TradeMesoPacket(amount).dispatch(); },
                    1, maximum, 1, true);
            }
            break;
        case BT_ENTER:
            send_chat();
            break;
        case BT_TRADE:
            if (!state.confirmed(false))
            {
                state.confirm(false);
                buttons[BT_TRADE]->set_state(Button::DISABLED);
                buttons[BT_COIN]->set_state(Button::DISABLED);
                TradeActionPacket(TradeActionPacket::CONFIRM).dispatch();
            }
            break;
        default:
            break;
        }
        return Button::PRESSED;
    }
}
