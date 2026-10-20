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
    class Char;
    class Player;
    struct Attack;

    // Damage and gating rules of the warrior 'Combo Attack' mechanic.
    // The orb counter itself is server-authoritative: every melee hit is
    // answered with GIVE_BUFF(COMBO) carrying the new orb count + 1 (the
    // centre orb), so the client only mirrors the server's damage maths.
    namespace combo_attack
    {
        // Return true for Coma/Panic, the finishers that consume the orbs.
        bool is_finisher(int32_t skill_id);

        // Return true if the finisher may be used. The combo value counts the
        // always-present centre orb, so at least one charged orb means > 1.
        bool can_use_finisher(int16_t combo_value);

        // Multiply the attack's damage range by the combo multiplier.
        void apply_damage(const Player& player, Attack& attack);

        // Return true if the advanced (Hero) orb appearance applies: the
        // advanced skill is learned or the counter passed the base orb cap.
        bool advanced_orbs(const Char& chr, int16_t value);
    }
}
