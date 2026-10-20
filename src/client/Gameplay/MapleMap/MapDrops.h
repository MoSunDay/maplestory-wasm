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
#include "DropLifecycle/RemovalPolicy.h"

#include "../Spawn.h"
#include "../../Graphics/Animation.h"
#include "../../Template/Rectangle.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
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
        };

        struct PetLootTarget
        {
            int32_t oid = 0;
            Point<int16_t> position;
        };

        PetLootTarget find_pet_loot(const PetLootQuery& query) const;
        bool has_drop(int32_t oid) const;
        bool has_lootable_drop(int32_t oid) const;
        // Collect the oids of lootable meso drops inside the range.
        std::vector<int32_t> collect_mesoids(
            const Rectangle<int16_t>& range) const;
        size_t size() const;
        uint32_t pet_pickup_count() const;

    private:
        void try_pickup(int32_t oid, const Drop& drop, Point<int16_t> playerpos,
            int64_t& closest_distance_sq, Loot& closest_loot) const;
        static bool pet_can_target(const Drop& drop, const PetLootQuery& query);

        MapObjects drops;

        enum MesoIcon
        {
            BRONZE, GOLD, BUNDLE, BAG,
            NUM_ICONS
        };
        std::array<Animation, NUM_ICONS> mesoicons;
        bool lootenabled;
        uint32_t confirmed_pet_pickups;

        std::queue<DropSpawn> spawns;
    };
}
