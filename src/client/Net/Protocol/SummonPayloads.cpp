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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
// GNU Affero General Public License for more details.                     //
#include "SummonPayloads.h"

#include "../Handlers/Helpers/MovementParser.h"
#include "../PacketError.h"

#include "../../Gameplay/MobCombat/DamageEncoding.h"

namespace jrc::summon_packets
{
    namespace
    {
        // Payload sizes as written by the server's MaplePacketCreator;
        // MOVE_SUMMON and SUMMON_ATTACK grow with their list counts.
        constexpr size_t SPAWN_LENGTH   = 24;
        constexpr size_t REMOVE_LENGTH  = 9;
        constexpr size_t MOVE_LENGTH    = 13;
        constexpr size_t ATTACK_LENGTH  = 11;
        constexpr size_t HIT_LENGTH     = 7;
        constexpr size_t DAMAGE_LENGTH  = 18;
        constexpr size_t DRAGON_LENGTH  = 16;
    }

    SpawnPayload parse_spawn(InPacket& recv)
    {
        if (recv.length() < SPAWN_LENGTH)
            throw PacketError("Truncated SPAWN_SPECIAL_MAPOBJECT payload");

        SpawnPayload payload;
        payload.owner_cid = recv.read_int();
        payload.oid = recv.read_int();
        payload.skillid = recv.read_int();
        recv.read_byte(); // 0x0A marker
        payload.skill_level = recv.read_byte();
        payload.position = recv.read_point();
        payload.stance = recv.read_byte();
        recv.read_short();
        payload.move_type = recv.read_byte();
        payload.puppet = recv.read_byte() == 0;
        payload.animated = recv.read_byte() == 0;
        return payload;
    }

    RemovePayload parse_remove(InPacket& recv)
    {
        if (recv.length() < REMOVE_LENGTH)
            throw PacketError("Truncated REMOVE_SPECIAL_MAPOBJECT payload");

        RemovePayload payload;
        payload.owner_cid = recv.read_int();
        payload.oid = recv.read_int();
        payload.animated = recv.read_byte() == 4;
        return payload;
    }

    MovePayload parse_move(InPacket& recv)
    {
        if (recv.length() < MOVE_LENGTH)
            throw PacketError("Truncated MOVE_SUMMON payload");

        MovePayload payload;
        payload.cid = recv.read_int();
        payload.oid = recv.read_int();
        payload.start = recv.read_point();
        // parse_movements consumes the fragment count byte itself.
        payload.movements = MovementParser::parse_movements(recv);
        return payload;
    }

    AttackPayload parse_attack(InPacket& recv)
    {
        if (recv.length() < ATTACK_LENGTH)
            throw PacketError("Truncated SUMMON_ATTACK payload");

        AttackPayload payload;
        payload.cid = recv.read_int();
        payload.summon_oid = recv.read_int();
        recv.read_byte(); // char level placeholder
        payload.toleft = recv.read_byte() != 0;
        const uint8_t count = recv.read_byte();
        if (recv.length() < static_cast<size_t>(count) * HIT_LENGTH)
            throw PacketError("Truncated SUMMON_ATTACK damage list");

        payload.hits.reserve(count);
        for (uint8_t i = 0; i < count; i++)
        {
            const int32_t monster_oid = recv.read_int();
            recv.read_byte(); // 6 marker
            const damage_encoding::Decoded decoded =
                damage_encoding::decode(recv.read_int());
            payload.hits.push_back(
                { monster_oid, decoded.damage, decoded.critical });
        }
        return payload;
    }

    DamagePayload parse_damage(InPacket& recv)
    {
        if (recv.length() < DAMAGE_LENGTH)
            throw PacketError("Truncated DAMAGE_SUMMON payload");

        DamagePayload payload;
        payload.cid = recv.read_int();
        payload.oid = recv.read_int();
        recv.read_byte(); // 12 marker
        payload.damage = recv.read_int();
        payload.monster_oid_from = recv.read_int();
        recv.read_byte();
        return payload;
    }

    DragonPayload parse_dragon(InPacket& recv)
    {
        if (recv.length() < DRAGON_LENGTH)
            throw PacketError("Truncated SPAWN_DRAGON payload");

        DragonPayload payload;
        payload.owner_cid = recv.read_int();
        const int16_t xpos = recv.read_short();
        recv.read_short();
        const int16_t ypos = recv.read_short();
        recv.read_short();
        payload.stance = recv.read_byte();
        recv.read_byte();
        payload.job = recv.read_short();
        payload.position = { xpos, ypos };
        return payload;
    }
}
