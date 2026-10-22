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
        : UIDragElement<PosTRADE>({ trade_layout::WIDTH, 24 }),
          inventory(inv), local_slot(slot), page(0),
          chatfield(Text::A11M, Text::LEFT, Text::DARKGREY,
              trade_layout::CHATFIELD, 70),
          page_label(Text::A11M, Text::RIGHT, Text::DARKGREY, "1-5 / 9",
              trade_layout::PAGE_WIDTH)
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

        names[0] = { Text::A12B, Text::CENTER, Text::DARKGREY, "等待中",
            trade_layout::NAME_WIDTH };
        names[1] = { Text::A12B, Text::CENTER, Text::DARKGREY, "等待对方",
            trade_layout::NAME_WIDTH };
        chatfield.set_enter_callback([this](const std::string&) { send_chat(); });
        dimension = { trade_layout::WIDTH, trade_layout::HEIGHT };
        active = true;
    }

    void UITrade::draw(float alpha) const
    {
        draw_sprites(alpha);
        names[0].draw(position + Point<int16_t>(trade_layout::LOCAL_NAME_X,
            trade_layout::NAME_Y));
        names[1].draw(position + Point<int16_t>(trade_layout::PARTNER_NAME_X,
            trade_layout::NAME_Y));
        if (looks[0])
            looks[0]->draw(position + Point<int16_t>(trade_layout::LOCAL_LOOK_X,
                trade_layout::LOOK_Y), false, Stance::STAND1, Expression::DEFAULT);
        if (looks[1])
            looks[1]->draw(position + Point<int16_t>(trade_layout::PARTNER_LOOK_X,
                trade_layout::LOOK_Y), true, Stance::STAND1, Expression::DEFAULT);

        draw_offers(false);
        draw_offers(true);
        page_label.draw(position + Point<int16_t>(trade_layout::PAGE_X,
            trade_layout::PAGE_Y));

        const std::string local_meso = "金币: " + std::to_string(state.meso(false));
        const std::string partner_meso = "金币: " + std::to_string(state.meso(true));
        Text(Text::A11B, Text::LEFT, Text::DARKGREY, local_meso)
            .draw(position + Point<int16_t>(trade_layout::LOCAL_MESO_X,
                trade_layout::MESO_Y));
        Text(Text::A11B, Text::LEFT, Text::DARKGREY, partner_meso)
            .draw(position + Point<int16_t>(trade_layout::PARTNER_MESO_X,
                trade_layout::MESO_Y));

        if (state.confirmed(false))
            Text(Text::A11B, Text::CENTER, Text::RED, "已确认",
                trade_layout::CONFIRM_WIDTH)
                .draw(position + Point<int16_t>(trade_layout::LOCAL_CONFIRM_X,
                    trade_layout::CONFIRM_Y));
        if (state.confirmed(true))
            Text(Text::A11B, Text::CENTER, Text::RED, "对方已确认",
                trade_layout::CONFIRM_WIDTH)
                .draw(position + Point<int16_t>(trade_layout::PARTNER_CONFIRM_X,
                    trade_layout::CONFIRM_Y));

        int16_t chat_y = trade_layout::CHAT_Y;
        for (const std::string& message : state.chat())
        {
            Text(Text::A11M, Text::LEFT, Text::DARKGREY, message,
                trade_layout::CHAT_WIDTH)
                .draw(position + Point<int16_t>(trade_layout::CHAT_X, chat_y));
            chat_y += trade_layout::CHAT_STEP;
        }
        chatfield.draw(position);
        draw_buttons(alpha);
    }

    void UITrade::draw_offers(bool partner) const
    {
        const int16_t x = partner ? trade_layout::PARTNER_ITEM_X :
            trade_layout::LOCAL_ITEM_X;
        const uint8_t first = page == 0 ? 1 : 5;
        for (uint8_t row = 0; row < 5; ++row)
        {
            uint8_t slot = first + row;
            const TradeOffer& offer = state.item(partner, slot);
            if (offer.item_id == 0)
                continue;

            Point<int16_t> item_position = position + Point<int16_t>(x,
                trade_layout::ITEM_TOP + row * trade_layout::ITEM_STEP);
            ItemData::get(offer.item_id).get_icon(false).draw(item_position);
            std::string label = ItemData::get(offer.item_id).get_name();
            if (offer.quantity > 1)
                label += " x" + std::to_string(offer.quantity);
            Text(Text::A11M, Text::LEFT, Text::DARKGREY, label,
                trade_layout::ITEM_LABEL_WIDTH)
                .draw(item_position + Point<int16_t>(trade_layout::ITEM_LABEL_DX,
                    trade_layout::ITEM_LABEL_DY));
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
