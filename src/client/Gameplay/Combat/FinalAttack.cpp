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
#include "FinalAttack.h"

#include "../../Character/SkillBook.h"
#include "../../Util/Misc.h"
#include "../../Util/Randomizer.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <string>

namespace jrc
{
    namespace
    {
        nl::node skill_node(int32_t skillid)
        {
            std::string strid = string_format::extend_id(skillid, 7);
            return nl::nx::skill[strid.substr(0, 3) + ".img"]["skill"][strid];
        }

        bool matches_weapon(int32_t fa_skill, Weapon::Type weapon)
        {
            switch (fa_skill)
            {
            case 1100002: // Fighter - sword
            case 1200003: // Page - sword
                return weapon == Weapon::SWORD_1H || weapon == Weapon::SWORD_2H;
            case 1100003: // Fighter - axe
                return weapon == Weapon::AXE_1H || weapon == Weapon::AXE_2H;
            case 1200002: // Page - blunt weapon
                return weapon == Weapon::MACE_1H || weapon == Weapon::MACE_2H;
            case 1300002: // Spearman - spear
                return weapon == Weapon::SPEAR;
            case 1300003: // Spearman - polearm
                return weapon == Weapon::POLEARM;
            default:
                return false;
            }
        }
    }

    namespace final_attack
    {
        int32_t find_skill(int32_t move_id, const Skillbook& book,
            Weapon::Type weapon)
        {
            if (move_id == 0)
                return 0;

            nl::node finalattacks = skill_node(move_id)["finalAttack"];
            if (!finalattacks)
                return 0;

            for (auto entry : finalattacks)
            {
                int32_t fa_skill =
                    string_conversion::or_default<int32_t>(entry.name(), 0);
                if (fa_skill <= 0)
                    continue;
                if (book.get_level(fa_skill) <= 0)
                    continue;
                if (!matches_weapon(fa_skill, weapon))
                    continue;
                if (!skill_node(fa_skill))
                    continue;
                return fa_skill;
            }
            return 0;
        }

        bool roll(int32_t fa_skill, const Skillbook& book)
        {
            int32_t level = book.get_level(fa_skill);
            if (level <= 0)
                return false;

            nl::node prop =
                skill_node(fa_skill)["level"][std::to_string(level)]["prop"];
            if (prop.data_type() != nl::node::type::integer)
                return false;

            int32_t percent = prop.get_integer();
            if (percent <= 0)
                return false;

            static Randomizer randomizer;
            return randomizer.below(percent / 100.0f);
        }
    }
}
