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

#include "DragTypes.h"
#include "Components/Icon.h"

namespace jrc
{
    // Cursor travel (in pixels) required before a press turns into a drag.
    constexpr int16_t DRAG_THRESHOLD = 4;

    // Value snapshot of an in-flight drag. The icon owns a cloned Icon::Type,
    // so the source window may mutate or drop its own icon while the drag is
    // still on screen. Move-only because Icon is move-only.
    struct DragSession
    {
        Icon icon;
        DragKind kind = DragKind::NONE;
        DragOrigin origin = DragOrigin::NONE;
        Point<int16_t> press_position;
        Point<int16_t> cursor_offset;
        bool active = false;
    };

    // True once the cursor moved at least DRAG_THRESHOLD pixels away from the
    // press position along either axis (Chebyshev distance).
    bool drag_threshold_reached(Point<int16_t> press, Point<int16_t> current);

    // Only world items and equipped gear may be released onto the stage;
    // skills and key bindings outside of a window are simply cancelled.
    bool drag_can_drop_on_stage(DragKind kind);
}
