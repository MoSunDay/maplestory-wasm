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

#include "Resolution.h"

namespace jrc
{
    namespace viewport
    {
        // Apply the persisted resolution before the GL window exists.
        // Updates engine constants and the browser canvas; does not touch
        // GL resources (they are created afterwards).
        void apply_boot(display::Resolution resolution);

        // Apply a resolution change while the game is running.
        // Updates engine constants, browser canvas, glViewport and the
        // GraphicsGL screen-size uniform.
        void apply_runtime(display::Resolution resolution);
    }
}
