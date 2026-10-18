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
#include "../../Gameplay/Physics/Physics.h"
#include "../../Gameplay/Pet/PetFollowPolicy.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Animation.h"
#include "../../Template/EnumMap.h"
#include "../../Template/Point.h"

#include <cstdint>
#include <string>
#include <map>
#include <optional>

#include "../../Gameplay/Movement.h"

namespace jrc
{
    class PetLook
    {
    public:
        enum Stance : uint8_t
        {
            MOVE,
            STAND,
            JUMP,
            ALERT,
            PRONE,
            FLY,
            HANG,
            WARP,
            LENGTH
        };

        static Stance stancebyvalue(uint8_t value)
        {
            uint8_t valueh = value / 2;
            return valueh >= LENGTH ? STAND : static_cast<Stance>(valueh);
        }

        PetLook(int32_t iid, std::string name, int32_t uniqueid,
            Point<int16_t> pos, uint8_t stance, int32_t fhid);
        PetLook();

        void draw(double viewx, double viewy, float alpha) const;
        void update(const Physics& physics, Point<int16_t> charpos);

        void set_position(int16_t xpos, int16_t ypos);
        void set_stance(Stance stance);
        void set_stance(uint8_t stancebyte);
        void set_loot_target(Point<int16_t> position);
        void clear_loot_target();
        void apply_movement(const Movement& movement);

        int32_t get_itemid() const;
        int32_t get_uniqueid() const;
        Point<int16_t> get_position() const;
        bool has_loot_target() const;
        int32_t get_last_warp_owner_distance() const;
        uint16_t get_foothold_id() const;
        bool is_onground() const;
        pet_follow::Action get_last_move_intent() const;
        Footholdtree::HorizontalCollision get_last_horizontal_collision() const;
        uint32_t get_jump_trigger_count() const;
        bool consume_urgent_movement();
        Movement get_movement() const;
        Stance get_stance() const;

    private:
        void reset_motion();
        void warp_to(Point<int16_t> position);
        void begin_jump(pet_follow::Action action, int32_t target_distance);
        void finish_jump(Point<int16_t> target_position);

        int32_t itemid = 0;
        std::string name;
        int32_t uniqueid = 0;
        Stance stance = STAND;
        bool flip = false;

        EnumMap<Stance, Animation> animations;
        PhysicsObject phobj;
        std::optional<Point<int16_t>> loot_target;
        Text namelabel;
        uint8_t failed_loot_jumps = 0;
        int32_t jump_start_distance = 0;
        int8_t jump_direction = 0;
        bool jump_airborne = false;
        int32_t last_warp_owner_distance = 0;
        pet_follow::Action last_move_intent = pet_follow::Action::IDLE;
        Footholdtree::HorizontalCollision last_horizontal_collision =
            Footholdtree::HorizontalCollision::NONE;
        uint32_t jump_trigger_count = 0;
        bool urgent_movement = false;
    };
}
