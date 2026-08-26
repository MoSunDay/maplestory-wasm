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
        static const double PETWALKFORCE = 0.35;
        static const double PETFLYFORCE = 0.2;

        Point<int16_t> curpos = phobj.get_position();
        if (stance == WARP)
        {
            physics.move_object(phobj);
            if (!animations[WARP].is_valid() || animations[WARP].update())
                set_stance(STAND);
            return;
        }

        switch (stance)
        {
        case STAND:
        case MOVE:
        {
            Point<int16_t> followpos = loot_target.value_or(charpos);
            int32_t follow_distance = curpos.distance(followpos);
            if (stance == MOVE)
            {
                if (pet_follow::made_progress(last_progress_distance, follow_distance))
                {
                    last_progress_distance = follow_distance;
                    stalled_frames = 0;
                }
                else if (stalled_frames < UINT16_MAX)
                {
                    ++stalled_frames;
                }
            }
            else
            {
                stalled_frames = 0;
                last_progress_distance = follow_distance;
            }

            pet_follow::Action action = pet_follow::decide({
                curpos, followpos, loot_target.has_value(), stance == MOVE, stalled_frames
            });
            if (action == pet_follow::Action::WARP)
            {
                Point<int16_t> destination = loot_target
                    ? followpos
                    : Point<int16_t>(static_cast<int16_t>(followpos.x() - 35), followpos.y());
                warp_to(destination);
                break;
            }
            if (action == pet_follow::Action::MOVE_RIGHT ||
                action == pet_follow::Action::MOVE_LEFT)
            {
                bool right = action == pet_follow::Action::MOVE_RIGHT;
                phobj.hforce = right ? PETWALKFORCE : -PETWALKFORCE;
                flip = right;
                set_stance(MOVE);
            }
            else
            {
                phobj.hforce = 0.0;
                phobj.hspeed = 0.0;
                phobj.hacc = 0.0;
                set_stance(STAND);
            }
            phobj.type = PhysicsObject::NORMAL;
            phobj.clear_flag(PhysicsObject::NOGRAVITY);
            break;
        }
        case HANG:
            set_position(charpos.x(), charpos.y());
            phobj.set_flag(PhysicsObject::NOGRAVITY);
            break;
        case FLY:
            if ((charpos - curpos).length() > 250)
            {
                set_position(charpos.x(), charpos.y());
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
            stalled_frames = 0;
            last_progress_distance = 0;
        }
        loot_target = position;
    }

    void PetLook::clear_loot_target()
    {
        loot_target.reset();
        last_progress_distance = 0;
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
        stalled_frames = 0;
        last_progress_distance = 0;
        urgent_movement = true;
        set_stance(WARP);
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
