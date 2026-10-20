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
#include "ComboAttack.h"

#include "Attack.h"

#include "../../Character/Buff.h"
#include "../../Character/Char.h"
#include "../../Character/Player.h"
#include "../../Data/SkillData.h"

namespace jrc
{
    namespace
    {
        // Crusader 'Combo Attack' and Hero 'Advanced Combo Attack'.
        constexpr int32_t COMBO_ATTACK   = 1111002;
        constexpr int32_t ADVANCED_COMBO = 1120003;

        // Damage percent of a skill level. SkillData stores it as a factor,
        // so it is scaled back to the percent the server formula works with.
        double skill_damage(int32_t skill_id, int32_t level)
        {
            if (level <= 0)
                return 0.0;

            return static_cast<double>(
                SkillData::get(skill_id).get_stats(level).damage) * 100.0;
        }

        // Skill level to use, with the server's max-level fallback for
        // characters whose book has no entry (e.g. GM-applied buffs).
        // Player::get_skilllevel is the const accessor of
        // Skillbook::get_level, so it works on a const Player&.
        int32_t level_or_max(const Player& player, int32_t skill_id)
        {
            int32_t level = player.get_skilllevel(skill_id);
            if (level > 0)
                return level;

            return SkillData::get(skill_id).get_masterlevel();
        }

        // Combo multiplier of one attack, mirroring the COMBO block of
        // AbstractDealDamageHandler#parseDamage on the server.
        double combo_factor(const Player& player, int16_t value, bool finisher)
        {
            if (value <= 0)
                return 1.0;

            // The counter includes the centre orb, the rest are charged orbs.
            const int16_t orbs = static_cast<int16_t>(value - 1);

            double factor;
            if (value > 6)
            {
                // Advanced Combo: (damage + 50)%, +20% flat and +4% for
                // every orb past five.
                const int32_t adv_level = level_or_max(player, ADVANCED_COMBO);
                factor = (skill_damage(ADVANCED_COMBO, adv_level) + 50.0) / 100.0
                    + 0.20 + (value - 5) * 0.04;
            }
            else
            {
                // Normal Combo: (damage + 50)% plus 1% per orb for every
                // 6 skill levels (the server uses integer division).
                const int32_t combo_level = level_or_max(player, COMBO_ATTACK);
                factor = (skill_damage(COMBO_ATTACK, combo_level) + 50.0) / 100.0
                    + (orbs * (combo_level / 6)) / 100.0;
            }

            if (!finisher)
                return factor;

            // Finishers scale with the number of charged orbs.
            if (orbs == 2)
                return factor * 1.2;
            if (orbs == 3)
                return factor * 1.54;
            if (orbs == 4)
                return factor * 2.0;
            if (orbs >= 5)
                return factor * 2.5;

            return factor;
        }
    }

    bool combo_attack::is_finisher(int32_t skill_id)
    {
        // Sword/Axe Panic and Coma, plus the 8-digit variants of later
        // versions that the server also treats as finishers.
        return (skill_id > 1111002 && skill_id < 1111007)
            || skill_id == 11111002 || skill_id == 11111003;
    }

    bool combo_attack::can_use_finisher(int16_t combo_value)
    {
        return combo_value > 1;
    }

    void combo_attack::apply_damage(const Player& player, Attack& attack)
    {
        const int16_t value = player.get_visual_buff_value(Buffstat::COMBO);
        if (value <= 0)
            return;

        const double factor = combo_factor(player, value, is_finisher(attack.skill));
        if (factor == 1.0)
            return;

        attack.mindamage *= factor;
        attack.maxdamage *= factor;
    }

    bool combo_attack::advanced_orbs(const Char& chr, int16_t value)
    {
        // GIVE_BUFF always names the base combo skill as the source, so the
        // appearance is derived from the skill book and the counter instead.
        // The extra orb that Advanced Combo can add is not predicted locally:
        // the server rolls `prop` and sends the resulting count back with
        // GIVE_BUFF(COMBO), so the client simply follows that value.
        return chr.get_skilllevel(ADVANCED_COMBO) > 0 || value > 6;
    }
}
