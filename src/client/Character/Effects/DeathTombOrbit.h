#pragma once

#include <cmath>
#include <cstdint>

#include "../../Template/Point.h"

namespace jrc::death_orbit
{
    constexpr int16_t CENTER_Y = -18;
    constexpr int16_t RADIUS_X = 32;
    constexpr int16_t RADIUS_Y = 12;

    inline Point<int16_t> offset(float angle)
    {
        return {
            static_cast<int16_t>(std::lround(RADIUS_X * std::sin(angle))),
            static_cast<int16_t>(std::lround(CENTER_Y - RADIUS_Y * std::cos(angle)))
        };
    }

    inline bool in_front(float angle)
    {
        return std::cos(angle) < 0.0f;
    }
}
