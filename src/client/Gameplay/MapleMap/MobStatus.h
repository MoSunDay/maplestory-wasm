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
#include <cstddef>
#include <cstdint>

#include "../../Graphics/Color.h"

namespace jrc
{
    namespace mob_status
    {
        // MonsterStatus first-mask bits (verified against the server's
        // MonsterStatus).
        constexpr uint32_t STUN_MASK   = 0x80;
        constexpr uint32_t FREEZE_MASK = 0x100;
        constexpr uint32_t POISON_MASK = 0x200;

        // Return a tint applied to a mob sprite while status effects hold.
        inline Color tint(uint32_t first_mask)
        {
            if (first_mask & POISON_MASK)
                return { 0.62f, 1.0f, 0.55f, 1.0f };
            if (first_mask & FREEZE_MASK)
                return { 0.60f, 0.78f, 1.0f, 1.0f };
            return Color::WHITE;
        }
    }
}
