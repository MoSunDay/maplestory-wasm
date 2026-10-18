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
#include "PetLook.h"

#include "../../Constants.h"
#include "../../Gameplay/Pet/PetFollowPolicy.h"

#include "nlnx/nx.hpp"
#include "nlnx/node.hpp"

#include <algorithm>
#include <utility>

namespace jrc
{
    PetLook::PetLook(int32_t iid, std::string nm, int32_t uqid,
        Point<int16_t> pos, uint8_t st, int32_t fhid) {

        itemid   = iid;
        name     = std::move(nm);
        uniqueid = uqid;
        stance = stancebyvalue(st);
        flip = st % 2 == 1;
        set_position(pos.x(), pos.y());
        phobj.fhid = static_cast<uint16_t>(std::max(fhid, 0));

        namelabel = { Text::A13M, Text::CENTER, Text::WHITE, Text::NAMETAG, name };

        std::string strid = std::to_string(iid);

        nl::node src = nl::nx::item["Pet"][strid + ".img"];

        animations[MOVE]  = src["move"];
        animations[STAND] = src["stand0"];
        animations[JUMP]  = src["jump"];
        animations[ALERT] = src["alert"];
        animations[PRONE] = src["prone"];
        animations[FLY]   = src["fly"];
        animations[HANG]  = src["hang"];

        nl::node effsrc = nl::nx::effect["PetEff.img"][strid];

        animations[WARP] = effsrc["warp"];
        animations[stance].reset();
    }

    PetLook::PetLook()
    {
        name = "";
    }

    void PetLook::draw(double viewx, double viewy, float alpha) const
    {
        Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);

        animations[stance].draw(DrawArgument(absp, flip), alpha);
        namelabel.draw(absp);
    }

    void PetLook::update(const Physics& physics, Point<int16_t> charpos)
    {
        static const double PETFLYFORCE = 0.2;
        static const double PETJUMPFORCE = 4.5;

        Point<int16_t> curpos = phobj.get_position();
        if (stance == WARP)
        {
            physics.move_object(phobj);
            if (!animations[WARP].is_valid() || animations[WARP].update())
                set_stance(STAND);
            return;
        }

        if (stance != HANG &&
            pet_follow::outside_owner_leash(curpos, charpos))
        {
            last_warp_owner_distance = pet_follow::distance_between(
                curpos, charpos);
            clear_loot_target();
            warp_to(pet_follow::player_side_position(charpos));
            physics.move_object(phobj);
            return;
        }

        switch (stance)
        {
        case STAND:
        case MOVE:
        {
            Point<int16_t> followpos = loot_target.value_or(charpos);
            int32_t follow_distance = pet_follow::distance_between(
                curpos, followpos);

            pet_follow::Action action = pet_follow::decide({
                curpos, charpos, followpos, loot_target.has_value(),
                stance == MOVE, failed_loot_jumps
            });
            if (action == pet_follow::Action::WARP_TO_OWNER)
            {
                last_warp_owner_distance = pet_follow::distance_between(
                    curpos, charpos);
                clear_loot_target();
                warp_to(pet_follow::player_side_position(charpos));
                physics.move_object(phobj);
                return;
            }
            if (action == pet_follow::Action::ABANDON_LOOT)
            {
                clear_loot_target();
                reset_motion();
                set_stance(STAND);
                last_move_intent = pet_follow::Action::IDLE;
                last_horizontal_collision =
                    Footholdtree::HorizontalCollision::NONE;
                physics.move_object(phobj);
                animations[stance].update();
                return;
            }
            if (action == pet_follow::Action::MOVE_RIGHT ||
                action == pet_follow::Action::MOVE_LEFT)
            {
                bool right = action == pet_follow::Action::MOVE_RIGHT;
                phobj.hforce = right
                    ? pet_follow::GROUND_WALK_FORCE
                    : -pet_follow::GROUND_WALK_FORCE;
                flip = right;
                set_stance(MOVE);
                last_move_intent = action;
            }
            else
            {
                phobj.hforce = 0.0;
                phobj.hspeed = 0.0;
                phobj.hacc = 0.0;
                set_stance(STAND);
                last_move_intent = pet_follow::Action::IDLE;
            }
            phobj.type = PhysicsObject::NORMAL;
            phobj.clear_flag(PhysicsObject::NOGRAVITY);
            phobj.clear_flag(PhysicsObject::TURNATEDGES);

            const Physics::MoveResult result = physics.move_object(phobj);
            last_horizontal_collision = result.horizontal_collision;
            const bool blocked_by_wall = result.horizontal_collision ==
                Footholdtree::HorizontalCollision::WALL;
            if (pet_follow::should_jump({ action, result.grounded,
                blocked_by_wall, result.start_x, result.end_x }))
            {
                begin_jump(action, follow_distance);
                phobj.vforce = -PETJUMPFORCE;
                ++jump_trigger_count;
            }
            animations[stance].update();
            return;
        }
        case JUMP:
        {
            const Point<int16_t> followpos = loot_target.value_or(charpos);
            if (jump_direction != 0)
                phobj.hspeed = jump_direction *
                    pet_follow::JUMP_HORIZONTAL_SPEED;
            phobj.type = PhysicsObject::NORMAL;
            phobj.clear_flag(PhysicsObject::NOGRAVITY);
            const Physics::MoveResult result = physics.move_object(phobj);
            const pet_follow::JumpProgress progress =
                pet_follow::advance_jump(jump_airborne, result.grounded);
            jump_airborne = progress.airborne;
            if (progress.landed)
                finish_jump(followpos);
            animations[stance].update();
            return;
        }
        case HANG:
            set_position(charpos.x(), charpos.y());
            phobj.set_flag(PhysicsObject::NOGRAVITY);
            break;
        case FLY:
            if (pet_follow::outside_owner_leash(curpos, charpos))
            {
                last_warp_owner_distance = pet_follow::distance_between(
                    curpos, charpos);
                clear_loot_target();
                warp_to(pet_follow::player_side_position(charpos));
                break;
            }
            else
            {
                if (charpos.x() - curpos.x() > 50)
                {
                    phobj.hforce = PETFLYFORCE;
                    flip = true;
                }
                else if (charpos.x() - curpos.x() < -50)
                {
                    phobj.hforce = -PETFLYFORCE;
                    flip = false;
                }
                else
                {
                    phobj.hforce = 0.0f;
                }

                if (charpos.y() - curpos.y() > 50.0f)
                {
                    phobj.vforce = PETFLYFORCE;
                }
                else if (charpos.y() - curpos.y() < -50.0f)
                {
                    phobj.vforce = -PETFLYFORCE;
                }
                else
                {
                    phobj.vforce = 0.0f;
                }
            }
            phobj.type = PhysicsObject::FLYING;
            phobj.clear_flag(PhysicsObject::NOGRAVITY);
            break;
        default:
            break;
        }

        physics.move_object(phobj);

        if (stance != WARP)
            animations[stance].update();
    }

    void PetLook::set_position(int16_t x, int16_t y)
    {
        phobj.set_x(x);
        phobj.set_y(y);
    }

    void PetLook::set_stance(Stance st)
    {
        if (stance != st)
        {
            stance = st;
            animations[stance].reset();
        }
    }

    void PetLook::set_stance(uint8_t stancebyte)
    {
        flip = stancebyte % 2 == 1;
        set_stance(stancebyvalue(stancebyte));
    }

    void PetLook::set_loot_target(Point<int16_t> position)
    {
        if (!loot_target || loot_target->x() != position.x() || loot_target->y() != position.y())
        {
            failed_loot_jumps = 0;
        }
        loot_target = position;
    }

    void PetLook::clear_loot_target()
    {
        loot_target.reset();
        failed_loot_jumps = 0;
        jump_start_distance = 0;
        jump_direction = 0;
        jump_airborne = false;
    }

    void PetLook::apply_movement(const Movement& movement)
    {
        set_position(movement.xpos, movement.ypos);
        phobj.fhid = movement.fh;
        set_stance(movement.newstate);
    }

    int32_t PetLook::get_itemid() const
    {
        return itemid;
    }

    int32_t PetLook::get_uniqueid() const
    {
        return uniqueid;
    }

    Point<int16_t> PetLook::get_position() const
    {
        return phobj.get_position();
    }

    bool PetLook::has_loot_target() const
    {
        return loot_target.has_value();
    }

    int32_t PetLook::get_last_warp_owner_distance() const
    {
        return last_warp_owner_distance;
    }

    uint16_t PetLook::get_foothold_id() const
    {
        return phobj.fhid;
    }

    bool PetLook::is_onground() const
    {
        return phobj.onground;
    }

    pet_follow::Action PetLook::get_last_move_intent() const
    {
        return last_move_intent;
    }

    Footholdtree::HorizontalCollision PetLook::get_last_horizontal_collision() const
    {
        return last_horizontal_collision;
    }

    uint32_t PetLook::get_jump_trigger_count() const
    {
        return jump_trigger_count;
    }

    bool PetLook::consume_urgent_movement()
    {
        bool result = urgent_movement;
        urgent_movement = false;
        return result;
    }

    void PetLook::reset_motion()
    {
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;
        phobj.hacc = 0.0;
        phobj.vacc = 0.0;
        phobj.hspeed = 0.0;
        phobj.vspeed = 0.0;
    }

    void PetLook::warp_to(Point<int16_t> position)
    {
        set_position(position.x(), position.y());
        reset_motion();
        phobj.type = PhysicsObject::NORMAL;
        phobj.clear_flag(PhysicsObject::NOGRAVITY);
        failed_loot_jumps = 0;
        jump_start_distance = 0;
        jump_direction = 0;
        jump_airborne = false;
        last_move_intent = pet_follow::Action::IDLE;
        last_horizontal_collision = Footholdtree::HorizontalCollision::NONE;
        urgent_movement = true;
        set_stance(WARP);
    }

    void PetLook::begin_jump(pet_follow::Action action, int32_t target_distance)
    {
        jump_direction = action == pet_follow::Action::MOVE_LEFT ? -1 : 1;
        flip = jump_direction > 0;
        phobj.hforce = 0.0;
        phobj.hacc = 0.0;
        phobj.hspeed = jump_direction * pet_follow::JUMP_HORIZONTAL_SPEED;
        jump_start_distance = target_distance;
        jump_airborne = false;
        phobj.clear_flag(PhysicsObject::TURNATEDGES);
        set_stance(JUMP);
    }

    void PetLook::finish_jump(Point<int16_t> target_position)
    {
        const int32_t current_distance = pet_follow::distance_between(
            get_position(), target_position);
        if (loot_target)
        {
            if (pet_follow::made_progress(jump_start_distance, current_distance))
                failed_loot_jumps = 0;
            else if (failed_loot_jumps < UINT8_MAX)
                ++failed_loot_jumps;
        }
        jump_start_distance = 0;
        jump_direction = 0;
        jump_airborne = false;
        set_stance(MOVE);
    }

    Movement PetLook::get_movement() const
    {
        uint8_t state = static_cast<uint8_t>(stance) * 2 + (flip ? 1 : 0);
        return Movement(phobj, state);
    }

    PetLook::Stance PetLook::get_stance() const
    {
        return stance;
    }
}
