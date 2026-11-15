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
#include "ChatBarGeometry.h"

namespace jrc
{
    namespace chatbar_geometry
    {
        ChatLayout chat_layout_for_screen(
            Generation generation, int16_t view_height, int16_t rows)
        {
            return {
                bar_bottom(view_height),
                clamp_rows(rows),
                chat_tail(generation),
                chat_width(generation)
            };
        }

        bool chatbox_hit(
            bool open, const ChatLayout& layout, Point<int16_t> point)
        {
            return open && chatbox_rect(layout).contains(point);
        }

        bool statusbar_hit_test(
            const HitTest& controls, bool chat_open,
            Rectangle<int16_t> chatbox, Point<int16_t> point)
        {
            return hit_any(controls, point)
                || (chat_open && chatbox.contains(point));
        }
    }
}
