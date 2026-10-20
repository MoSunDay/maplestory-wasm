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
#include "../../Character/Inventory/Weapon.h"

#include <cstdint>

namespace jrc
{
    class Skillbook;

    // Helpers for the warrior passive 'Final Attack', which can trigger a
    // second close-range attack after a primary melee skill hit.
    namespace final_attack
    {
        // Return the final-attack skill id that can follow the primary skill
        // `move_id` for a player with the given skill book and equipped
        // weapon, or 0 when none applies. Only skills whose FA variant the
        // player has learned and that match the equipped weapon family are
        // returned.
        int32_t find_skill(int32_t move_id, const Skillbook& book,
            Weapon::Type weapon);

        // Roll the level-based proc chance of the final attack skill.
        bool roll(int32_t fa_skill, const Skillbook& book);
    }
}
