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
#include <unordered_map>

namespace jrc
{
    namespace Buffstat
    {
        enum Id
        {
            NONE,

            MORPH,
            RECOVERY,
            MAPLE_WARRIOR,
            STANCE,
            SHARP_EYES,
            MANA_REFLECTION,
            SHADOW_CLAW,
            INFINITY_,
            HOLY_SHIELD,
            HAMSTRING,
            BLIND,
            CONCENTRATE,
            ECHO_OF_HERO,
            GHOST_MORPH,
            AURA,
            CONFUSE,
            BERSERK_FURY,
            DIVINE_BODY,
            SPARK,
            FINALATTACK,
            BATTLESHIP,
            WATK,
            WDEF,
            MATK,
            MDEF,
            ACC,
            AVOID,
            HANDS,
            SHOWDASH,
            SPEED,
            JUMP,
            MAGIC_GUARD,
            DARKSIGHT,
            BOOSTER,
            POWERGUARD,
            HYPERBODYHP,
            HYPERBODYMP,
            INVINCIBLE,
            SOULARROW,
            STUN,
            POISON,
            SEAL,
            DARKNESS,
            COMBO,
            SUMMON,
            WK_CHARGE,
            DRAGONBLOOD,
            HOLY_SYMBOL,
            MESOUP,
            SHADOWPARTNER,
            PICKPOCKET,
            PUPPET,
            MESOGUARD,
            WEAKEN,

            DASH,
            DASH2,
            ELEMENTAL_RESET,
            ARAN_COMBO,
            COMBO_DRAIN,
            COMBO_BARRIER,
            BODY_PRESSURE,
            SMART_KNOCKBACK,
            PYRAMID_PQ,
            ENERGY_CHARGE,
            MONSTER_RIDING,
            HOMING_BEACON,
            SPEED_INFUSION,
            POTION_LOCK,
            SHADOW,
            FEAR,
            SEDUCE,
            ZOMBIFY,
            SLOW,
            CURSE,

            LENGTH
        };

        extern const std::unordered_map<Id, uint64_t> first_codes;
        extern const std::unordered_map<Id, uint64_t> second_codes;
    }

    // GIVE_BUFF duration semantics: positive values count down, zero or
    // negative means the buff lasts until the server cancels it.
    constexpr int32_t buff_remaining_from_duration(int32_t duration)
    {
        return duration > 0 ? duration : -1;
    }

    struct Buff
    {
        Buffstat::Id stat;
        int16_t value;
        int32_t skillid;
        int32_t duration;
        // 0 = inactive, <0 = until cancelled, >0 = countdown in milliseconds.
        int32_t remaining_ms;

        constexpr Buff(Buffstat::Id stat, int16_t value, int32_t skillid, int32_t duration)
            : stat(stat), value(value), skillid(skillid), duration(duration),
              remaining_ms(buff_remaining_from_duration(duration)) {}

        constexpr Buff()
            : stat(Buffstat::NONE), value(0), skillid(0), duration(0),
              remaining_ms(0) {}
    };

    constexpr bool buff_is_active(const Buff& buff)
    {
        return buff.remaining_ms != 0;
    }
}
