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

#include <cstdint>

namespace jrc
{
    // Value tag describing what a drag carries. Used by drop targets to
    // decide whether they accept a dragged icon before it is released.
    enum class DragKind : uint8_t
    {
        NONE,
        ITEM,
        EQUIP,
        SKILL,
        KEYBINDING
    };

    // Window that started the drag. When that window disappears while the
    // drag is still in flight the session has to be cancelled with it.
    enum class DragOrigin : uint8_t
    {
        NONE,
        ITEMINVENTORY,
        EQUIPINVENTORY,
        SKILLBOOK,
        KEYCONFIG
    };
}
