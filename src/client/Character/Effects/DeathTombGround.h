#pragma once

#include <cmath>
#include <cstdint>

#include "../../Template/Point.h"

namespace jrc::death_tomb_ground
{
    /// Convert Physics::get_y_below's collision-safe position back to the
    /// actual foothold line used as the tomb texture's bottom anchor.
    inline Point<int16_t> landing_anchor(
        Point<int16_t> death_position,
        Point<int16_t> position_above_ground)
    {
        return {
            death_position.x(),
            static_cast<int16_t>(position_above_ground.y() + 1)
        };
    }

    /// Project a fixed world anchor through the interpolated camera offset.
    inline Point<int16_t> absolute_position(
        Point<int16_t> world_position,
        double view_x,
        double view_y)
    {
        return {
            static_cast<int16_t>(std::lround(world_position.x() + view_x)),
            static_cast<int16_t>(std::lround(world_position.y() + view_y))
        };
    }
}
