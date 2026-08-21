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
#include "MapNpcs.h"

#include "Npc.h"

#include "../../Net/Packets/NpcInteractionPackets.h"

#include <limits>

namespace jrc
{
    void MapNpcs::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        npcs.draw(layer, viewx, viewy, alpha);
    }

    void MapNpcs::update(const Physics& physics)
    {
        instantiate_spawns(physics);
        npcs.update(physics);
    }

    void MapNpcs::instantiate_spawns(const Physics& physics)
    {
        for (; !spawns.empty(); spawns.pop())
        {
            const NpcSpawn& spawn = spawns.front();

            int32_t oid = spawn.get_oid();
            Optional<MapObject> npc = npcs.get(oid);
            if (npc)
            {
                npc->makeactive();
            }
            else
            {
                npcs.add(
                    spawn.instantiate(physics)
                );
            }
        }

    }

    void MapNpcs::spawn(NpcSpawn&& spawn)
    {
        spawns.emplace(
            std::move(spawn)
        );
    }

    void MapNpcs::remove(int32_t oid)
    {
        if (auto npc = npcs.get(oid))
            npc->deactivate();
    }

    void MapNpcs::clear()
    {
        npcs.clear();
    }

    Cursor::State MapNpcs::send_cursor(bool pressed, Point<int16_t> position, Point<int16_t> viewpos)
    {
        for (auto& mmo : npcs)
        {
            Npc* npc = static_cast<Npc*>(mmo.second.get());
            if (npc && npc->is_active() && npc->inrange(position, viewpos))
            {
                if (pressed)
                {
                    // TODO: try finding dialogue first
                    TalkToNPCPacket(npc->get_oid())
                        .dispatch();
                    return Cursor::IDLE;
                }
                else
                {
                    return Cursor::CANCLICK;
                }
            }
        }
        return Cursor::IDLE;
    }

    bool MapNpcs::talk_to_nearest(Point<int16_t> player_position)
    {
        constexpr int32_t MAX_HORIZONTAL_DISTANCE = 120;
        constexpr int32_t MAX_VERTICAL_DISTANCE = 80;

        Npc* nearest = nullptr;
        int32_t nearest_distance = std::numeric_limits<int32_t>::max();
        for (auto& map_object : npcs)
        {
            Npc* npc = static_cast<Npc*>(map_object.second.get());
            if (!npc || !npc->accepts_keyboard_talk())
            {
                continue;
            }

            const Point<int16_t> position = npc->get_position();
            const int32_t horizontal = position.x() - player_position.x();
            const int32_t vertical = position.y() - player_position.y();
            if (horizontal < -MAX_HORIZONTAL_DISTANCE || horizontal > MAX_HORIZONTAL_DISTANCE ||
                vertical < -MAX_VERTICAL_DISTANCE || vertical > MAX_VERTICAL_DISTANCE)
            {
                continue;
            }

            const int32_t distance = horizontal * horizontal + vertical * vertical;
            if (distance < nearest_distance)
            {
                nearest = npc;
                nearest_distance = distance;
            }
        }

        if (!nearest)
        {
            return false;
        }

        TalkToNPCPacket(nearest->get_oid()).dispatch();
        return true;
    }
}
