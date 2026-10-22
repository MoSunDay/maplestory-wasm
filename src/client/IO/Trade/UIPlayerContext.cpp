#include "UIPlayerContext.h"

#include "../../Constants.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/TradePackets.h"

#include <algorithm>

namespace jrc
{
    UIPlayerContext::UIPlayerContext(const PlayerTarget& selected, Point<int16_t> cursor)
        : target(selected),
          background(player_context_layout::WIDTH,
              player_context_layout::HEIGHT, Geometry::BLACK, 0.86f),
          header(player_context_layout::WIDTH,
              player_context_layout::HEADER_HEIGHT, Geometry::WHITE, 0.12f),
          divider(player_context_layout::WIDTH, Geometry::WHITE, 0.35f),
          title(Text::A11B, Text::LEFT, Text::WHITE, selected.name),
          trade(Text::A11M, Text::LEFT, Text::WHITE, "交易"),
          party(Text::A11M, Text::LEFT, Text::WHITE, "组队邀请")
    {
        const int16_t max_x = std::max<int16_t>(0,
            Constants::viewwidth() - player_context_layout::WIDTH);
        const int16_t max_y = std::max<int16_t>(0,
            Constants::viewheight() - player_context_layout::HEIGHT);
        position = {
            std::clamp<int16_t>(cursor.x(), 0, max_x),
            std::clamp<int16_t>(cursor.y(), 0, max_y)
        };
        dimension = { player_context_layout::WIDTH,
            player_context_layout::HEIGHT };
        active = true;
    }

    void UIPlayerContext::draw(float) const
    {
        background.draw(position);
        header.draw(position);
        divider.draw(position + Point<int16_t>(0,
            player_context_layout::HEADER_HEIGHT));
        title.draw(position + Point<int16_t>(9, 5));
        trade.draw(position + Point<int16_t>(9,
            player_context_layout::TRADE_TEXT_Y));
        party.draw(position + Point<int16_t>(9,
            player_context_layout::PARTY_TEXT_Y));
    }

    bool UIPlayerContext::remove_cursor(bool clicked, Point<int16_t> cursor)
    {
        if (clicked && !is_in_range(cursor))
            active = false;
        return false;
    }

    UIElement::CursorResult UIPlayerContext::send_cursor(bool clicked, Point<int16_t> cursor)
    {
        if (!is_in_range(cursor))
            return { Cursor::IDLE, false };
        if (clicked)
        {
            using Action = player_context_layout::Action;
            const Action action = player_context_layout::action_at(
                cursor.y() - position.y());
            if (action == Action::TRADE)
            {
                CreateTradePacket().dispatch();
                InviteTradePacket(target.id).dispatch();
            }
            else if (action == Action::PARTY)
            {
                InviteToPartyPacket(target.name).dispatch();
            }
            else
            {
                return { Cursor::CANCLICK, true };
            }
            active = false;
            return { Cursor::CLICKING, true };
        }
        return { Cursor::CANCLICK, true };
    }

    void UIPlayerContext::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            active = false;
    }
}
