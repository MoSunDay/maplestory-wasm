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
#include "MapDrops.h"

#include "Drop.h"
#include "ItemDrop.h"
#include "MesoDrop.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <limits>

namespace jrc
{
    namespace
    {
        constexpr int16_t PICKUP_RADIUS = 30;
        constexpr int64_t PICKUP_RADIUS_SQ = PICKUP_RADIUS * PICKUP_RADIUS;

        int64_t squared_distance(Point<int16_t> first, Point<int16_t> second)
        {
            const int64_t dx = static_cast<int32_t>(first.x()) - second.x();
            const int64_t dy = static_cast<int32_t>(first.y()) - second.y();
            return dx * dx + dy * dy;
        }
    }

    MapDrops::MapDrops()
        : confirmed_pet_pickups(0)
    {
        lootenabled = false;
    }

    void MapDrops::init()
    {
        nl::node src = nl::nx::item["Special"]["0900.img"];

        mesoicons[BRONZE] = src["09000000"]["iconRaw"];
        mesoicons[GOLD] = src["09000001"]["iconRaw"];
        mesoicons[BUNDLE] = src["09000002"]["iconRaw"];
        mesoicons[BAG] = src["09000003"]["iconRaw"];
    }

    void MapDrops::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        drops.draw(layer, viewx, viewy, alpha);
    }

    void MapDrops::update(const Physics& physics)
    {
        instantiate_spawns();

        for (auto& mesoicon : mesoicons)
        {
            mesoicon.update();
        }

        drops.update(physics);

        lootenabled = true;
    }

    void MapDrops::instantiate_spawns()
    {
        for (; !spawns.empty(); spawns.pop())
        {
            const DropSpawn& spawn = spawns.front();

            int32_t oid = spawn.get_oid();
            if (Optional<Drop> drop = drops.get(oid))
            {
                drop->update_ownership(spawn.get_owner(), spawn.get_drop_type(), spawn.is_player_drop());
                drop->makeactive();
            }
            else
            {
                int32_t itemid = spawn.get_itemid();
                bool meso = spawn.is_meso();
                if (meso)
                {
                    MesoIcon mesotype = (itemid > 999) ? BAG : (itemid > 99) ? BUNDLE : (itemid > 49) ? GOLD : BRONZE;
                    const Animation& icon = mesoicons[mesotype];
                    drops.add(spawn.instantiate(icon));
                }
                else if (const ItemData& itemdata = ItemData::get(itemid))
                {
                    const Texture& icon = itemdata.get_icon(true);
                    drops.add(spawn.instantiate(icon));
                }
            }
        }

    }

    void MapDrops::spawn(DropSpawn&& spawn)
    {
        spawns.emplace(std::move(spawn));
    }

    void MapDrops::remove(int32_t oid, drop_removal::Effect effect,
        const PhysicsObject* looter,
        std::optional<Point<int16_t>> pickup_target, bool local_pet_pickup)
    {
        if (Optional<Drop> drop = drops.get(oid))
        {
            if (local_pet_pickup)
                ++confirmed_pet_pickups;
            if (effect == drop_removal::Effect::IMMEDIATE_ERASE)
            {
                drops.remove(oid);
                return;
            }
            drop->expire(effect, looter, pickup_target);
        }
    }

    void MapDrops::clear()
    {
        drops.clear();
        confirmed_pet_pickups = 0;
    }

    void MapDrops::try_pickup(int32_t oid, const Drop& drop,
        Point<int16_t> playerpos, int64_t& closest_distance_sq,
        Loot& closest_loot) const
    {
        if (!drop.is_lootable())
            return;

        Point<int16_t> position = drop.get_position();
        Point<int16_t> center = position + Point<int16_t>(16, 16);

        const int64_t distance_sq = squared_distance(center, playerpos);

        // Keep strict overlap, but also accept a small proximity window so
        // slightly offset drops can still be attempted during pickup spam.
        bool is_overlapping = drop.bounds().contains(playerpos);
        bool is_nearby = distance_sq <= PICKUP_RADIUS_SQ;
        if (!is_overlapping && !is_nearby)
        {
            return;
        }

        if (distance_sq >= closest_distance_sq)
        {
            return;
        }

        closest_distance_sq = distance_sq;
        closest_loot = { oid, position };
    }

    MapDrops::Loot MapDrops::find_loot_at(Point<int16_t> playerpos)
    {
        if (!lootenabled)
            return { 0, {} };

        int64_t closest_distance_sq = std::numeric_limits<int64_t>::max();
        Loot closest_loot = { 0, {} };

        for (auto& mmo : drops)
        {
            Optional<const Drop> drop = mmo.second.get();
            if (!drop)
            {
                continue;
            }
            try_pickup(mmo.first, *drop, playerpos, closest_distance_sq, closest_loot);
        }

        if (closest_loot.first)
        {
            lootenabled = false;
        }

        return closest_loot;
    }

    bool MapDrops::pet_can_target(const Drop& drop, const PetLootQuery& query)
    {
        if (drop.is_player_drop())
            return false;
        if (drop.is_meso() ? !query.meso_magnet : !query.item_pouch)
            return false;

        int32_t owner = drop.get_owner();
        bool free_for_all = owner <= 0 || drop.get_pickup_type() == 2 || drop.get_pickup_type() == 3;
        bool owned = owner == query.character_id || owner == query.party_id;
        return free_for_all || owned || query.magic_scales;
    }

    MapDrops::PetLootTarget MapDrops::find_pet_loot(const PetLootQuery& query) const
    {
        int64_t closest_distance_sq =
            static_cast<int64_t>(query.range) * query.range + 1;
        PetLootTarget target;
        for (const auto& entry : drops)
        {
            Optional<const Drop> drop = entry.second.get();
            if (!drop || !drop->is_lootable() || !pet_can_target(*drop, query))
                continue;

            Point<int16_t> position = drop->get_position() + Point<int16_t>(16, 16);
            const int64_t distance_sq = squared_distance(position, query.origin);
            if (distance_sq < closest_distance_sq)
            {
                closest_distance_sq = distance_sq;
                target = { entry.first, position };
            }
        }
        return target;
    }

    bool MapDrops::has_drop(int32_t oid) const
    {
        return static_cast<bool>(drops.get(oid));
    }

    bool MapDrops::has_lootable_drop(int32_t oid) const
    {
        Optional<const Drop> drop = drops.get(oid);
        return drop && drop->is_lootable();
    }

    size_t MapDrops::size() const
    {
        return drops.size();
    }

    uint32_t MapDrops::pet_pickup_count() const
    {
        return confirmed_pet_pickups;
    }
}
