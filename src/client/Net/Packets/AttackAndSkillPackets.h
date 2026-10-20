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
#include "../OutPacket.h"

#include "../../Gameplay/Combat/Attack.h"
#include "../../Gameplay/MobCombat/DamageEncoding.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace jrc
{
    // Notifies the server of an attack. The opcode is determined by the attack type.
    // Attack::CLOSE = CLOSE_ATTACK(44)
    // Attack::RANGED = RANGED_ATTACK(45)
    // Attack::MAGIC = MAGIC_ATTACK(46)
    class AttackPacket : public OutPacket
    {
    public:
        AttackPacket(const AttackResult& attack)
            : OutPacket(opcodefor(attack.type)) {

            skip(1);

            write_byte((std::min(attack.mobcount, uint8_t(15)) << 4)
                | std::min(attack.hitcount, uint8_t(15)));
            write_int(attack.skill);
            if (attack.charge > 0 || is_charge_skill(attack.skill))
                write_int(attack.charge);

            skip(8);

            write_byte(attack.display);
            write_byte(attack.toleft);
            write_byte(attack.stance);

            skip(1);

            write_byte(attack.speed);

            if (attack.type == Attack::RANGED)
            {
                skip(1);
                write_byte(attack.toleft);
                skip(7);
                if (is_ranged_extra_skill(attack.skill))
                    skip(4);
            }
            else
            {
                skip(4);
            }

            for (auto& damagetomob : attack.damagelines)
            {
                write_int(damagetomob.first);

                skip(14);

                for (auto& singledamage : damagetomob.second)
                {
                    write_int(damage_encoding::encode(
                        singledamage.first, singledamage.second));
                }

                skip(4);
            }
        }

    private:
        // Skills for which the server always reads the four byte charge,
        // even when it is zero.
        static bool is_charge_skill(int32_t skillid)
        {
            switch (skillid)
            {
            case 2121001: // FP ArchMage - Big Bang
            case 2221001: // IL ArchMage - Big Bang
            case 2321001: // Bishop - Big Bang
            case 5201002: // Gunslinger - Grenade
            case 5101004: // Brawler - Cork Screw Blow
            case 15101003: // Thunder Breaker - Cork Screw Blow
            case 14111006: // Night Walker - Poison Bomb
            case 22121000: // Evan - Ice Breath
            case 22151001: // Evan - Fire Breath
                return true;
            default:
                return false;
            }
        }

        // Ranged skills which send four additional bytes.
        static bool is_ranged_extra_skill(int32_t skillid)
        {
            switch (skillid)
            {
            case 3121004: // Bowmaster - Hurricane
            case 3221001: // Marksman - Piercing Arrow
            case 5221004: // Corsair - Rapid Fire
            case 13111002: // Wind Archer - Hurricane
                return true;
            default:
                return false;
            }
        }

        static OutPacket::Opcode opcodefor(Attack::Type type)
        {
            switch (type)
            {
            case Attack::CLOSE:
                return CLOSE_ATTACK;
            case Attack::RANGED:
                return RANGED_ATTACK;
            default:
                return MAGIC_ATTACK;
            }
        }
    };


    // Notifies the server of a Meso Explosion attack, which detonates meso
    // drops next to the attacked mobs. Daggers are close combat, so this
    // always uses the close attack opcode.
    // Opcode: CLOSE_ATTACK(44)
    class MesoExplosionPacket : public OutPacket
    {
    public:
        MesoExplosionPacket(const AttackResult& attack,
            const std::vector<int32_t>& mesoids)
            : OutPacket(CLOSE_ATTACK) {

            skip(1);

            const bool mesoonly = attack.damagelines.empty();
            if (mesoonly)
            {
                write_byte(0);
            }
            else
            {
                write_byte((std::min(attack.mobcount, uint8_t(15)) << 4)
                    | std::min(attack.hitcount, uint8_t(15)));
            }

            write_int(attack.skill);

            // The server reads no charge for Meso Explosion.

            skip(8);

            write_byte(attack.display);
            write_byte(attack.toleft);
            write_byte(attack.stance);

            if (mesoonly)
            {
                skip(10);

                write_mesoids(mesoids);
            }
            else
            {
                skip(6);

                for (auto& damagetomob : attack.damagelines)
                {
                    write_int(damagetomob.first);

                    skip(12);

                    write_byte(static_cast<uint8_t>(
                        damagetomob.second.size()));
                    for (auto& singledamage : damagetomob.second)
                    {
                        write_int(damage_encoding::encode(
                            singledamage.first, singledamage.second));
                    }

                    skip(4);
                }

                // The server reads one more oid before the meso list.
                skip(4);

                write_mesoids(mesoids);
            }
        }

    private:
        void write_mesoids(const std::vector<int32_t>& mesoids)
        {
            write_byte(static_cast<uint8_t>(mesoids.size()));
            for (int32_t mesoid : mesoids)
            {
                write_int(mesoid);
                skip(1);
            }
        }
    };


    // Tells the server that the player took damage.
    // Opcode: TAKE_DAMAGE(48)
    class TakeDamagePacket : public OutPacket
    {
    public:
        enum From : int8_t
        {
            TOUCH = -1,
            ATTACK_1 = 0
        };

        TakeDamagePacket(int8_t from, uint8_t element, int32_t damage,
            int32_t mobid, int32_t oid, uint8_t direction) : OutPacket(TAKE_DAMAGE) {

            write_time();
            write_byte(from);
            write_byte(element);
            write_int(damage);
            write_int(mobid);
            write_int(oid);
            write_byte(direction);
        }

        // From mob attack result.
        TakeDamagePacket(const MobAttackResult& result, From from)
            : TakeDamagePacket(from, 0, result.damage, result.mobid, result.oid, result.direction) {}

        TakeDamagePacket(const MobAttackResult& result, uint8_t attack_index)
            : TakeDamagePacket(static_cast<int8_t>(attack_index), 0, result.damage,
                               result.mobid, result.oid, result.direction) {}
    };


    // Packet which notifies the server of a skill usage.
    // Opcode: USE_SKILL(91)
    class UseSkillPacket : public OutPacket
    {
    public:
        UseSkillPacket(int32_t skillid, int32_t level) : OutPacket(USE_SKILL)
        {
            write_time();
            write_int(skillid);
            write_byte(static_cast<uint8_t>(level));

            // if monster magnet : some more bytes

            if (skillid % 10000000 == 1004)
                skip(2); // no idea what this could be

            // a point (4 bytes) could be added at the end
        }
    };
}
