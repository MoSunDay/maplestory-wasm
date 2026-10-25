//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../Template/Rectangle.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace jrc
{
    // Pure geometry of the warrior rush skills (Hero 1121006,
    // Paladin 1221007, Dark Knight 1321003). Rush always dashes forward
    // in the direction the attack faces; the endpoint only depends on the
    // attacker position, the facing direction, the skill's forward hitbox
    // extent and the viewport width. Which mobs were hit (if any) does not
    // influence it.
    namespace rush_movement
    {
        // Forward extent of the skill hitbox: lt.x is negative for ranges
        // that reach in front of the caster (-250 for a 250px reach), so
        // negating it yields a positive distance. hrange scales the base
        // range the same way MapMobs::send_attack does. Non-positive
        // extents mean "no reach" and are reported as 0.
        inline int32_t forward_reach(const Rectangle<int16_t>& range,
            float hrange)
        {
            const float reach = -static_cast<float>(range.l()) * hrange;
            if (reach <= 0.0f)
                return 0;
            const float max_reach =
                static_cast<float>(std::numeric_limits<int16_t>::max());
            if (reach >= max_reach)
                return std::numeric_limits<int16_t>::max();
            return static_cast<int32_t>(reach);
        }

        // Rush is capped to half of the current viewport width, so wider
        // windows allow longer dashes instead of a fixed distance.
        inline int16_t half_view_distance(int16_t view_width)
        {
            if (view_width <= 0)
                return 0;
            return static_cast<int16_t>(view_width / 2);
        }

        // Distance the dash may cover: the skill reach, capped by the
        // half-screen limit. A range with no forward extent (empty or
        // malformed NX data) falls back to the cap so the dash length
        // stays predictable.
        inline int16_t dash_distance(const Rectangle<int16_t>& range,
            float hrange, int16_t max_distance)
        {
            if (max_distance <= 0)
                return 0;

            const int32_t reach = forward_reach(range, hrange);
            if (reach <= 0)
                return max_distance;

            return static_cast<int16_t>(
                std::min<int32_t>(reach, max_distance));
        }

        // Move distance pixels to the right when facing right, otherwise
        // to the left; clamp so the result always fits an int16_t.
        inline int16_t target_x(int16_t origin_x, bool facing_right,
            int16_t distance)
        {
            const int32_t origin = static_cast<int32_t>(origin_x);
            const int32_t delta = static_cast<int32_t>(distance);
            const int32_t raw = facing_right ? origin + delta : origin - delta;

            const int32_t clamped = std::min<int32_t>(
                std::max<int32_t>(raw, std::numeric_limits<int16_t>::min()),
                std::numeric_limits<int16_t>::max());
            return static_cast<int16_t>(clamped);
        }

        // Endpoint of a rush: forward extent of the skill hitbox, capped to
        // half the viewport width, applied in the facing direction. Every
        // mob position and hit count is irrelevant here.
        inline int16_t forward_target(int16_t origin_x, bool facing_right,
            const Rectangle<int16_t>& range, float hrange, int16_t view_width)
        {
            const int16_t max_distance = half_view_distance(view_width);
            const int16_t distance = dash_distance(range, hrange, max_distance);
            return target_x(origin_x, facing_right, distance);
        }
    }
}
