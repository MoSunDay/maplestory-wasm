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
#include "../../../Template/Point.h"
#include "../../../Template/Rectangle.h"

#include <cstdint>
#include <vector>

namespace jrc::drop_loot
{
    // Radius around a drop's icon center within which pickup is offered.
    constexpr int16_t PICKUP_RADIUS = 30;

    struct Candidate
    {
        int32_t oid = 0;
        Point<int16_t> position;
        bool lootable = false;
    };

    inline int64_t squared_distance(Point<int16_t> first, Point<int16_t> second)
    {
        const int64_t dx = static_cast<int32_t>(first.x()) - second.x();
        const int64_t dy = static_cast<int32_t>(first.y()) - second.y();
        return dx * dx + dy * dy;
    }

    // The player can attempt a pickup when standing inside the 32x32 drop
    // icon bounds or near its center. Mirrors the previous Drop::bounds()
    // overlap check plus the nearby-radius window.
    inline bool in_pickup_reach(const Candidate& candidate, Point<int16_t> playerpos)
    {
        const Rectangle<int16_t> bounds(candidate.position,
            candidate.position + Point<int16_t>(32, 32));
        if (bounds.contains(playerpos))
            return true;

        const Point<int16_t> center = candidate.position + Point<int16_t>(16, 16);
        return squared_distance(center, playerpos) <=
            static_cast<int64_t>(PICKUP_RADIUS) * PICKUP_RADIUS;
    }

    // Picks the nearest lootable candidate in reach. `skip_oid` only loses
    // priority: it is still returned when it is the only reachable candidate,
    // so server-side drop locks (400ms spawn lock, ownership window) can be
    // retried instead of starving the pickup forever. Returns nullptr when
    // nothing is reachable.
    inline const Candidate* select_nearest(
        const std::vector<Candidate>& candidates, Point<int16_t> playerpos,
        int32_t skip_oid)
    {
        const Candidate* fallback = nullptr;
        int64_t fallback_distance_sq = 0;
        const Candidate* chosen = nullptr;
        int64_t closest_distance_sq = 0;

        for (const Candidate& candidate : candidates)
        {
            if (!candidate.lootable || !in_pickup_reach(candidate, playerpos))
                continue;

            const Point<int16_t> center =
                candidate.position + Point<int16_t>(16, 16);
            const int64_t distance_sq =
                squared_distance(center, playerpos);

            if (!fallback || distance_sq < fallback_distance_sq)
            {
                fallback = &candidate;
                fallback_distance_sq = distance_sq;
            }

            if (candidate.oid == skip_oid)
                continue;

            if (!chosen || distance_sq < closest_distance_sq)
            {
                chosen = &candidate;
                closest_distance_sq = distance_sq;
            }
        }

        return chosen ? chosen : fallback;
    }
}
