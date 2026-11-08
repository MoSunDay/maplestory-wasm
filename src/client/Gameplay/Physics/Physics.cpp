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
#include "Physics.h"
#include <functional>

namespace jrc
{
    const double GRAVFORCE     = Physics::GRAVITY;
    const double SWIMGRAVFORCE = 0.03;
    const double FRICTION      = 0.3;
    const double SLOPEFACTOR   = 0.1;
    // Constant downhill pull per unit foothold slope on snow maps. Positive
    // slope descends to the right (screen-y grows downward), so a resting
    // object keeps gaining downhill speed until fs-scaled friction balances
    // it -- the official El Nath slopes cannot be stood on without snowshoes.
    const double SNOWSLOPE     = 0.02;
    const double GROUNDSLIP    = 3.0;
    const double FLYFRICTION   = 0.05;
    const double SWIMFRICTION  = 0.08;

    Physics::Physics(nl::node src)
        : Physics(src, 0.0) {}

    Physics::Physics(nl::node src, double slipfactor)
    {
        fht  = src;
        slip = slipfactor;
    }

    Physics::Physics() = default;

    Physics::MoveResult Physics::move_object(PhysicsObject& phobj) const
    {
        MoveResult result;
        result.start_x = phobj.crnt_x();

        // Determine which platform the object is currently on.
        fht.update_fh(phobj);
        result.grounded = phobj.onground;

        // Use the appropriate physics for the terrain the object is on.
        switch (phobj.type)
        {
        case PhysicsObject::NORMAL:
            move_normal(phobj);
            result.horizontal_collision = fht.limit_movement(phobj);
            break;
        case PhysicsObject::FLYING:
            move_flying(phobj);
            result.horizontal_collision = fht.limit_movement(phobj);
            break;
        case PhysicsObject::SWIMMING:
            move_swimming(phobj);
            result.horizontal_collision = fht.limit_movement(phobj);
            break;
        case PhysicsObject::FIXATED:
            break;
        case PhysicsObject::ICE:
            move_ice(phobj);
            result.horizontal_collision = fht.limit_movement(phobj);
            break;
        }

        // Move the object forward.
        phobj.move();
        result.end_x = phobj.crnt_x();
        return result;
    }

    void Physics::move_normal(PhysicsObject& phobj) const
    {
        phobj.vacc = 0.0;
        phobj.hacc = 0.0;
        if (phobj.onground)
        {
            phobj.vacc += phobj.vforce;
            phobj.hacc += phobj.hforce;

            if (phobj.hacc == 0.0 && phobj.hspeed < 0.1 && phobj.hspeed > -0.1)
            {
                phobj.hspeed = 0.0;
            }
            else
            {
                double inertia = phobj.hspeed / GROUNDSLIP;
                double slopef = phobj.fhslope;
                if (slopef > 0.5)
                {
                    slopef = 0.5;
                }
                else if (slopef < -0.5)
                {
                    slopef = -0.5;
                }
                phobj.hacc -= (FRICTION + SLOPEFACTOR * (1.0 + slopef * -inertia)) * inertia;
            }
        }
        else if (phobj.is_flag_not_set(PhysicsObject::NOGRAVITY))
        {
            phobj.vacc += GRAVFORCE;
        }
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;

        phobj.hspeed += phobj.hacc;
        phobj.vspeed += phobj.vacc;
    }

    void Physics::move_ice(PhysicsObject& phobj) const
    {
        // Official snow maps ('info/fs', 0.2 on every El Nath field) work as
        // a friction multiplier plus a slope pull, replicating the official
        // behaviour:
        //  - while a walk force is applied, full ground grip keeps walking
        //    exactly like on normal ground (snow never changes walk speed);
        //  - once the input is released, the map's slip factor scales the
        //    whole ground friction down (fs=0.2 -> one fifth of the normal
        //    grip), so the object glides on much longer before stopping;
        //  - on sloped ground a constant downhill pull keeps the object
        //    sliding and drags a resting object downhill, so slopes cannot
        //    be stood on (or climbed after stopping) without snowshoes.
        // Snowshoes keep NORMAL physics, which cancels all of this.
        phobj.vacc = 0.0;
        phobj.hacc = 0.0;
        if (phobj.onground)
        {
            phobj.vacc += phobj.vforce;
            phobj.hacc += phobj.hforce;

            bool walking = phobj.hacc != 0.0;
            double slopef = phobj.fhslope;
            if (slopef > 0.5)
            {
                slopef = 0.5;
            }
            else if (slopef < -0.5)
            {
                slopef = -0.5;
            }

            phobj.hacc += SNOWSLOPE * slopef;

            bool resting = !walking
                && slopef == 0.0
                && phobj.hspeed < 0.1 && phobj.hspeed > -0.1;
            if (resting)
            {
                phobj.hspeed = 0.0;
            }
            else
            {
                double inertia = phobj.hspeed / GROUNDSLIP;
                double grip = FRICTION + SLOPEFACTOR * (1.0 + slopef * -inertia);
                phobj.hacc -= (walking ? grip : slip * grip) * inertia;
            }
        }
        else if (phobj.is_flag_not_set(PhysicsObject::NOGRAVITY))
        {
            phobj.vacc += GRAVFORCE;
        }
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;

        phobj.hspeed += phobj.hacc;
        phobj.vspeed += phobj.vacc;
    }

    void Physics::move_flying(PhysicsObject& phobj) const
    {
        phobj.hacc   = phobj.hforce;
        phobj.vacc   = phobj.vforce;
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;

        phobj.hacc -= FLYFRICTION * phobj.hspeed;
        phobj.vacc -= FLYFRICTION * phobj.vspeed;

        phobj.hspeed += phobj.hacc;
        phobj.vspeed += phobj.vacc;

        if (phobj.hacc == 0.0 && phobj.hspeed < 0.1 && phobj.hspeed > -0.1)
        {
            phobj.hspeed = 0.0;
        }

        if (phobj.vacc == 0.0 && phobj.vspeed < 0.1 && phobj.vspeed > -0.1)
        {
            phobj.vspeed = 0.0;
        }
    }

    void Physics::move_swimming(PhysicsObject& phobj) const
    {
        phobj.hacc   = phobj.hforce;
        phobj.vacc   = phobj.vforce;
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;

        phobj.hacc -= SWIMFRICTION * phobj.hspeed;
        phobj.vacc -= SWIMFRICTION * phobj.vspeed;

        if (phobj.is_flag_not_set(PhysicsObject::NOGRAVITY))
        {
            phobj.vacc += SWIMGRAVFORCE;
        }

        phobj.hspeed += phobj.hacc;
        phobj.vspeed += phobj.vacc;

        if (phobj.hacc == 0.0 && phobj.hspeed < 0.1 && phobj.hspeed > -0.1)
        {
            phobj.hspeed = 0.0;
        }
        if (phobj.vacc == 0.0 && phobj.vspeed < 0.1 && phobj.vspeed > -0.1)
        {
            phobj.vspeed = 0.0f;
        }
    }

    Point<int16_t> Physics::get_y_below(Point<int16_t> position) const
    {
        int16_t ground = fht.get_y_below(position);
        return Point<int16_t>(position.x(), ground - 1);
    }

    const Footholdtree& Physics::get_fht() const
    {
        return fht;
    }
}
