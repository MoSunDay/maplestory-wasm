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
#include "MapHazards.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/nx.hpp"

namespace
{
    constexpr uint8_t MAP_LAYER_COUNT = 8;

    // Resolve the Obj definition node for one placed obj entry. Returns a
    // null node when the path does not exist in the Obj tree.
    nl::node resolve_obj_definition(nl::node objsrc)
    {
        const std::string objset = objsrc["oS"] + ".img";
        return nl::nx::map["Obj"][objset][objsrc["l0"]][objsrc["l1"]][objsrc["l2"]];
    }

    // First frame of an Obj definition. Trap definitions name it "0".
    nl::node first_bitmap_frame(nl::node def)
    {
        if (nl::node frame = def["0"]
            ; frame && frame.data_type() == nl::node::type::bitmap)
        {
            return frame;
        }
        for (const nl::node child : def)
        {
            if (child.data_type() == nl::node::type::bitmap)
                return child;
        }
        return {};
    }
}

namespace jrc
{
    std::vector<MapHazard> collect_map_hazards(nl::node mapsrc)
    {
        std::vector<MapHazard> hazards;
        for (uint8_t layer = 0; layer < MAP_LAYER_COUNT; ++layer)
        {
            const nl::node objs = mapsrc[std::to_string(layer)]["obj"];
            if (!objs)
                continue;

            for (const nl::node objsrc : objs)
            {
                const nl::node def = resolve_obj_definition(objsrc);
                const int32_t obstacle = def["obstacle"];
                const int32_t damage = def["damage"];
                if (obstacle < 1 || damage < 1)
                    continue;

                // Anchor and footprint come from the first sprite frame,
                // mirroring how the visual Obj is placed on the map.
                const nl::node frame = first_bitmap_frame(def);
                if (!frame)
                    continue;

                const nl::bitmap bitmap = frame;
                const Point<int16_t> origin{ frame["origin"] };
                const Point<int16_t> dimensions{
                    bitmap.width(),
                    bitmap.height()
                };
                const Point<int16_t> pos{ objsrc["x"], objsrc["y"] };
                const bool flip = objsrc["f"].get_bool();
                hazards.push_back({
                    hazard_bounds(pos, flip, origin, dimensions),
                    damage
                });
            }
        }
        return hazards;
    }
}
