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
#include "DragSession.h"

#include <cstdlib>

namespace jrc
{
    bool drag_threshold_reached(Point<int16_t> press, Point<int16_t> current)
    {
        // Chebyshev distance: a drag starts once either axis travelled far
        // enough. Squared euclidean distance would let a fast diagonal flick
        // of (3, 3) start a drag, which is further than intended for clicks.
        const int dx = std::abs(static_cast<int>(current.x()) - static_cast<int>(press.x()));
        const int dy = std::abs(static_cast<int>(current.y()) - static_cast<int>(press.y()));
        return dx >= DRAG_THRESHOLD || dy >= DRAG_THRESHOLD;
    }

    bool drag_can_drop_on_stage(DragKind kind)
    {
        return kind == DragKind::ITEM || kind == DragKind::EQUIP;
    }
}
