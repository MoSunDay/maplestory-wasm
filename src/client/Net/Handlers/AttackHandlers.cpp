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
#include "AttackHandlers.h"

#include "../../Character/SkillId.h"
#include "../../Gameplay/MobCombat/DamageEncoding.h"
#include "../../Gameplay/Stage.h"

#include <unordered_map>
#include <vector>

namespace
{
    // The magic broadcast carries a trailing charge int only for these
    // skills. The gate is MagicDamageHandler.java, which passes
    // attack.charge only for FPArchMage.BIG_BANG, ILArchMage.BIG_BANG,
    // Bishop.BIG_BANG, Evan.ICE_BREATH and Evan.FIRE_BREATH (constants
    // under src/constants/skills/), while MaplePacketCreator
    // magicAttack() appends writeInt(charge) only when charge != -1.
    // Keep this list in sync with those constants; other charge skills
    // broadcast through the close/ranged paths without the extra int.
    bool has_broadcast_charge(int32_t skillid)
    {
        switch (skillid)
        {
        case 2121001: // FPArchMage.BIG_BANG
        case 2221001: // ILArchMage.BIG_BANG
        case 2321001: // Bishop.BIG_BANG
        case 22121000: // Evan.ICE_BREATH
        case 22151001: // Evan.FIRE_BREATH
            return true;
        default:
            return false;
        }
    }
}

namespace jrc
{
    AttackHandler::AttackHandler(Attack::Type t)
    {
        type = t;
    }

    void AttackHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        uint8_t count = recv.read_byte();

        recv.skip(1);

        AttackResult attack;
        attack.type = type;
        attack.attacker = cid;

        attack.level = recv.read_byte();
        attack.skill = (attack.level > 0) ? recv.read_int() : 0;

        attack.display = recv.read_byte();
        attack.toleft = recv.read_bool();
        attack.stance = recv.read_byte();
        attack.speed = recv.read_byte();

        recv.skip(1);

        attack.bullet = recv.read_int();

        attack.mobcount = (count >> 4) & 0xF;
        attack.hitcount = count & 0xF;
        for (uint8_t i = 0; i < attack.mobcount; i++)
        {
            int32_t oid = recv.read_int();

            recv.skip(1);

            uint8_t length = (attack.skill == SkillId::MESO_EXPLOSION) ? recv.read_byte() : attack.hitcount;
            for (uint8_t j = 0; j < length; j++)
            {
                const damage_encoding::Decoded decoded =
                    damage_encoding::decode(recv.read_int());
                auto singledamage = std::make_pair(decoded.damage, decoded.critical);
                attack.damagelines[oid].push_back(singledamage);
            }
        }

        // Trailing fields: rangedAttack() always appends one int (0) and
        // magicAttack() appends the charge int for breath/Big Bang skills.
        if (type == Attack::RANGED)
        {
            recv.skip(4);
        }
        else if (type == Attack::MAGIC && has_broadcast_charge(attack.skill))
        {
            attack.charge = recv.read_int();
        }

        Stage::get().get_combat().push_attack(attack);
    }
}
