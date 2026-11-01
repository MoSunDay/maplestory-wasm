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
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "nlnx/node.hpp"

#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"

#include <cstdint>
#include <vector>

namespace jrc
{
    // Damage applied by obstacle objs whose definition marks `obstacle` but
    // ships no `damage` value (the steam/gas jets of the Ellinia dungeon
    // "迷宫" maps). Matches the 100 of the jet's sibling design families.
    constexpr int32_t DEFAULT_OBSTACLE_DAMAGE = 100;

    // A damaging map object (地刺/陷阱): the sprite footprint of a placed obj
    // in map coordinates plus the fixed damage its Obj definition declares.
    struct MapHazard
    {
        Rectangle<int16_t> bounds;
        int32_t damage;
    };

    // Footprint of one placed obj frame. Mirrors the draw rectangle of the
    // sprite anchored at `pos`, including the horizontal flip flag.
    inline Rectangle<int16_t> hazard_bounds(Point<int16_t> pos, bool flip,
        Point<int16_t> origin, Point<int16_t> dimensions)
    {
        const int16_t left = pos.x() - origin.x();
        const int16_t right = left + dimensions.x();
        const int16_t top = pos.y() - origin.y();
        const int16_t bottom = top + dimensions.y();
        if (!flip)
            return { left, right, top, bottom };

        // Flip mirrors the sprite around the anchor's x position.
        const int16_t mirror_left = 2 * pos.x() - right;
        const int16_t mirror_right = 2 * pos.x() - left;
        return { mirror_left, mirror_right, top, bottom };
    }

    // Footprint of one placed obj frame from its authored hit box. `lt`/`rb`
    // are the collision vectors of the sprite frame relative to its anchor.
    inline Rectangle<int16_t> hazard_bounds_from_hitbox(Point<int16_t> pos,
        bool flip, Point<int16_t> lt, Point<int16_t> rb)
    {
        const int16_t left = pos.x() + lt.x();
        const int16_t right = pos.x() + rb.x();
        const int16_t top = pos.y() + lt.y();
        const int16_t bottom = pos.y() + rb.y();
        if (!flip)
            return { left, right, top, bottom };

        // Flip mirrors the hit box around the anchor's x position.
        const int16_t mirror_left = 2 * pos.x() - right;
        const int16_t mirror_right = 2 * pos.x() - left;
        return { mirror_left, mirror_right, top, bottom };
    }

    // First hazard whose footprint overlaps the player rectangle, or nullptr.
    inline const MapHazard* find_map_hazard_hit(
        const std::vector<MapHazard>& hazards,
        const Rectangle<int16_t>& player_bounds)
    {
        for (const MapHazard& hazard : hazards)
        {
            if (hazard.bounds.overlaps(player_bounds))
                return &hazard;
        }
        return nullptr;
    }

    // Walk the map image layers and collect every damaging obstacle. An obj
    // is a hazard when its definition node (Map.nx `Obj/<oS>.img/<l0>/<l1>/
    // <l2>`) carries `obstacle` >= 1; `damage` defaults to
    // DEFAULT_OBSTACLE_DAMAGE when the definition has no `damage` >= 1.
    std::vector<MapHazard> collect_map_hazards(nl::node mapsrc);
}
