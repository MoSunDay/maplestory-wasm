#include "UIPlayerContext.h"

#include "../../Constants.h"
#include "../../Net/Packets/TradePackets.h"

#include <algorithm>

namespace jrc
{
    UIPlayerContext::UIPlayerContext(const PlayerTarget& selected, Point<int16_t> cursor)
        : target(selected),
          background(150, 56, Geometry::BLACK, 0.86f),
          header(150, 24, Geometry::WHITE, 0.12f),
          divider(150, Geometry::WHITE, 0.35f),
          title(Text::A11B, Text::LEFT, Text::WHITE, selected.name),
          trade(Text::A11M, Text::LEFT, Text::WHITE, "交易")
    {
        const int16_t max_x = std::max<int16_t>(0, Constants::viewwidth() - 150);
        const int16_t max_y = std::max<int16_t>(0, Constants::viewheight() - 56);
        position = {
            std::clamp<int16_t>(cursor.x(), 0, max_x),
            std::clamp<int16_t>(cursor.y(), 0, max_y)
        };
        dimension = { 150, 56 };
        active = true;
    }

    void UIPlayerContext::draw(float) const
    {
        background.draw(position);
        header.draw(position);
        divider.draw(position + Point<int16_t>(0, 24));
        title.draw(position + Point<int16_t>(9, 5));
        trade.draw(position + Point<int16_t>(9, 33));
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
        if (clicked && cursor.y() >= position.y() + 24)
        {
            CreateTradePacket().dispatch();
            InviteTradePacket(target.id).dispatch();
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
