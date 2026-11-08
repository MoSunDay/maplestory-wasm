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
#include "FootholdTree.h"

namespace jrc
{
    // Class that uses physics engines and the collection of platforms to determine object movement.
    class Physics
    {
    public:
        // Shared downward acceleration for NORMAL physics, in pixels per frame^2.
        static constexpr double GRAVITY = 0.14;

        struct MoveResult
        {
            double start_x = 0.0;
            double end_x = 0.0;
            bool grounded = false;
            Footholdtree::HorizontalCollision horizontal_collision =
                Footholdtree::HorizontalCollision::NONE;
        };

        Physics(nl::node src);
        // 'slipfactor' is the map's 'fs' value (0.2 on all official snow
        // maps). For ICE physics it scales the whole ground friction down
        // while no walk force is applied (the inertia slide after the input
        // is released) -- walk-force-driven movement keeps full grip so
        // walking stays identical to normal ground.
        Physics(nl::node src, double slipfactor);
        Physics();

        // Move the specified object over the specified game-time.
        MoveResult move_object(PhysicsObject& tomove) const;
        // Determine the point on the ground below the specified position.
        Point<int16_t> get_y_below(Point<int16_t> position) const;
        // Return a reference to the collection of platforms.
        const Footholdtree& get_fht() const;

    private:
        void move_normal(PhysicsObject&) const;
        void move_ice(PhysicsObject&) const;
        void move_flying(PhysicsObject&) const;
        void move_swimming(PhysicsObject&) const;

        Footholdtree fht;
        double slip = 0.0;
    };
}
