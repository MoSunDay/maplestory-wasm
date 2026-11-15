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
#include "UIStateGame.h"

#include <utility>

namespace jrc
{
    namespace
    {
        // The window type that owns a drag origin. Used to cancel a drag
        // when its source window closes mid-flight.
        UIElement::Type type_for_origin(DragOrigin origin)
        {
            switch (origin)
            {
            case DragOrigin::ITEMINVENTORY:
                return UIElement::ITEMINVENTORY;
            case DragOrigin::EQUIPINVENTORY:
                return UIElement::EQUIPINVENTORY;
            case DragOrigin::SKILLBOOK:
                return UIElement::SKILLBOOK;
            case DragOrigin::KEYCONFIG:
                return UIElement::KEYCONFIG;
            case DragOrigin::NONE:
                break;
            }
            return UIElement::NONE;
        }
    }

    Cursor::State UIStateGame::update_drag(Cursor::State mst, Point<int16_t> pos)
    {
        if (!draggedicon)
        {
            return Cursor::IDLE;
        }

        DragSession& drag = *draggedicon;
        if (mst == Cursor::IDLE)
        {
            if (drag.active)
            {
                drop_icon(drag, pos);
            }

            draggedicon.reset();
            // The global drag icon consumes the mouse-up, so release the
            // source window's capture here; otherwise the next click is
            // swallowed by it.
            cursor_captured = UIElement::NONE;
            return Cursor::IDLE;
        }

        if (!drag.active && drag_threshold_reached(drag.press_position, pos))
        {
            drag.active = true;
            drag.icon.start_drag(drag.cursor_offset);
        }

        return Cursor::GRABBING;
    }

    void UIStateGame::drop_icon(DragSession& drag, Point<int16_t> pos)
    {
        if (UIElement* front = get_front(pos))
        {
            // A window under the cursor owns the release: either it accepts
            // the payload or the drop is silently cancelled. Never fall
            // through to the stage below an open window.
            if (front->accepts_drop(drag.kind, pos))
            {
                front->send_icon(drag.icon, pos);
            }
            return;
        }

        if (drag_can_drop_on_stage(drag.kind))
        {
            drag.icon.drop_on_stage();
        }
    }

    void UIStateGame::cancel_drag_if_origin(UIElement::Type type)
    {
        if (!draggedicon)
        {
            return;
        }

        if (type_for_origin(draggedicon->origin) == type)
        {
            draggedicon.reset();
        }
    }

    void UIStateGame::cancel_drag()
    {
        // Double-click actions can mutate inventory before mouse-up arrives,
        // so the transient value snapshot is simply discarded.
        draggedicon.reset();
    }

    void UIStateGame::drag_icon(Icon icon, DragKind kind, DragOrigin origin,
        Point<int16_t> cursor_offset, Point<int16_t> press_position)
    {
        draggedicon.emplace(DragSession{
            std::move(icon), kind, origin, press_position, cursor_offset, false
        });
    }
}
