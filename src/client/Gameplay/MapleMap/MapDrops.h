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
#include "MapObjects.h"
#include "Drop.h"
#include "DropLifecycle/LootSelection.h"
#include "DropLifecycle/RemovalPolicy.h"

#include "../Spawn.h"
#include "../Pet/PetFollowPolicy.h"
#include "../../Graphics/Animation.h"
#include "../../Template/Rectangle.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

namespace jrc
{
    class Drop;

    class MapDrops
    {
    public:
        MapDrops();

        // Initialize the meso icons.
        void init();

        // Draw all drops on a layer.
        void draw(Layer::Id layer, double viewx, double viewy, float alpha) const;
        // Update all drops.
        void update(const Physics& physics);
        void instantiate_spawns();

        // Spawn a new drop.
        void spawn(DropSpawn&& spawn);
        // Remove a drop.
        void remove(int32_t oid, drop_removal::Effect effect,
            const PhysicsObject* looter,
            std::optional<Point<int16_t>> pickup_target = std::nullopt,
            bool local_pet_pickup = false);
        // Remove all drops.
        void clear();

        // Find a drop which can be picked up at the specified position.
        using Loot = std::pair<int32_t, Point<int16_t>>;
        Loot find_loot_at(Point<int16_t> playerpos);

        struct PetLootQuery
        {
            Point<int16_t> origin;
            int32_t character_id = 0;
            int32_t party_id = -1;
            int16_t range = 150;
            bool meso_magnet = false;
            bool item_pouch = false;
            bool magic_scales = false;
            // Loot higher than this above the query origin is not targeted; the
            // owner cannot reach it with a single jump either.
            int32_t max_height_above = pet_follow::pursuit_height_limit(
                pet_follow::BASE_MOVEMENT);
        };

        struct PetLootTarget
        {
            int32_t oid = 0;
            Point<int16_t> position;
        };

        PetLootTarget find_pet_loot(const PetLootQuery& query) const;
        // All pet-eligible loot within range of origin, nearest first. Loot
        // higher than the query's height limit above origin is skipped.
        std::vector<int32_t> find_pet_loot_batch(const PetLootQuery& query,
            Point<int16_t> origin, int16_t range) const;
        // Icon center of the lootable drop nearest to `origin`, or nullopt
        // when no drop is lootable. Pure read; changes no state.
        std::optional<Point<int16_t>> find_nearest_lootable_center(
            Point<int16_t> origin) const
        {
            std::optional<Point<int16_t>> nearest;
            int64_t nearest_distance_sq = 0;
            for (const auto& entry : drops)
            {
                Optional<const Drop> drop = entry.second.get();
                if (!drop || !drop->is_lootable())
                    continue;

                const Point<int16_t> center =
                    drop->get_position() + Point<int16_t>(16, 16);
                const int64_t distance_sq =
                    drop_loot::squared_distance(center, origin);
                if (!nearest || distance_sq < nearest_distance_sq)
                {
                    nearest = center;
                    nearest_distance_sq = distance_sq;
                }
            }
            return nearest;
        }

        bool has_drop(int32_t oid) const;
        bool has_lootable_drop(int32_t oid) const;
        // Collect the oids of lootable meso drops inside the range.
        std::vector<int32_t> collect_mesoids(
            const Rectangle<int16_t>& range) const;
        size_t size() const;
        uint32_t pet_pickup_count() const;

    private:
        static bool pet_can_target(const Drop& drop, const PetLootQuery& query);

        MapObjects drops;

        enum MesoIcon
        {
            BRONZE, GOLD, BUNDLE, BAG,
            NUM_ICONS
        };
        std::array<Animation, NUM_ICONS> mesoicons;
        bool lootenabled;
        int32_t last_loot_oid;
        uint32_t confirmed_pet_pickups;

        std::queue<DropSpawn> spawns;
        // Removals that arrived before their queued spawn was instantiated.
        std::unordered_set<int32_t> pending_removals;
    };
}
