//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright (C) 2015-2016 Daniel Allendorf                                 //
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
    namespace magic_damage
    {
        // Skill.nx stores magic attack as "mad", which the server reads as a
        // percentage (AbstractDealDamageHandler does
        // calcDmgMax *= effect.getMatk() / 100). The "damage" percent of a
        // magic skill is not applied at all, including the custom wide magic
        // skills.
        constexpr double scale(int32_t matk)
        {
            return matk / 100.0;
        }

        constexpr double apply(double base, int32_t matk)
        {
            return base * scale(matk);
        }
    }
}
