#pragma once

#include <cstdint>

namespace jrc
{
    namespace player_context_layout
    {
        constexpr int16_t WIDTH = 150;
        constexpr int16_t HEADER_HEIGHT = 24;
        constexpr int16_t ROW_HEIGHT = 24;
        constexpr int16_t HEIGHT = HEADER_HEIGHT + ROW_HEIGHT * 2;

        constexpr int16_t TRADE_TEXT_Y = HEADER_HEIGHT + 9;
        constexpr int16_t PARTY_TEXT_Y = HEADER_HEIGHT + ROW_HEIGHT + 9;

        enum class Action
        {
            NONE,
            TRADE,
            PARTY
        };

        constexpr Action action_at(int16_t y)
        {
            if (y < HEADER_HEIGHT || y >= HEIGHT)
                return Action::NONE;
            return y < HEADER_HEIGHT + ROW_HEIGHT ? Action::TRADE : Action::PARTY;
        }
    }
}
