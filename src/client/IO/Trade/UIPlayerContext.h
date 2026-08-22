#pragma once

#include "../UIElement.h"

#include "../../Gameplay/Targeting/PlayerTargeting.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"

namespace jrc
{
    class UIPlayerContext : public UIElement
    {
    public:
        static constexpr Type TYPE = PLAYER_CONTEXT;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UIPlayerContext(const PlayerTarget& target, Point<int16_t> cursor);

        void draw(float alpha) const override;
        bool remove_cursor(bool clicked, Point<int16_t> cursor) override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursor) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

    private:
        PlayerTarget target;
        ColorBox background;
        ColorBox header;
        ColorLine divider;
        Text title;
        Text trade;
    };
}
