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
#pragma once
#include "../InPacket.h"

#include "../../Gameplay/Movement.h"

#include "../../Template/Point.h"
#include <cstdint>
#include <vector>

namespace jrc::summon_packets
{
    // Fields of SPAWN_SPECIAL_MAPOBJECT(175): ownerId, oid, skill,
    // 0x0A marker, skill level, position, stance, filler short,
    // movement type, puppet flag (0 = puppet), animated flag (0 = yes).
    struct SpawnPayload
    {
        int32_t owner_cid = 0;
        int32_t oid = 0;
        int32_t skillid = 0;
        uint8_t skill_level = 0;
        Point<int16_t> position;
        uint8_t stance = 0;
        uint8_t move_type = 0;
        bool puppet = false;
        bool animated = false;
    };

    // Fields of REMOVE_SPECIAL_MAPOBJECT(176): ownerId, oid and the
    // removal mode, where 4 means "animated death".
    struct RemovePayload
    {
        int32_t owner_cid = 0;
        int32_t oid = 0;
        bool animated = false;
    };

    // Fields of MOVE_SUMMON(177): cid, oid, start position and the
    // movement fragment list.
    struct MovePayload
    {
        int32_t cid = 0;
        int32_t oid = 0;
        Point<int16_t> start;
        std::vector<Movement> movements;
    };

    // One damage line of SUMMON_ATTACK(178), already decoded.
    struct Hit
    {
        int32_t monster_oid = 0;
        int32_t damage = 0;
        bool critical = false;
    };

    struct AttackPayload
    {
        int32_t cid = 0;
        int32_t summon_oid = 0;
        bool toleft = false;
        std::vector<Hit> hits;
    };

    // Fields of DAMAGE_SUMMON(179): cid, oid, 12 marker, damage, the
    // monster the damage came from and a trailing byte.
    struct DamagePayload
    {
        int32_t cid = 0;
        int32_t oid = 0;
        int32_t damage = 0;
        int32_t monster_oid_from = 0;
    };

    // Fields of SPAWN_DRAGON(181): ownerId, padded position, stance and
    // the owner's job.
    struct DragonPayload
    {
        int32_t owner_cid = 0;
        Point<int16_t> position;
        uint8_t stance = 0;
        int16_t job = 0;
    };

    // Each parser validates the payload length and throws PacketError on
    // truncation, mirroring the other protocol payload modules.
    SpawnPayload parse_spawn(InPacket& recv);
    RemovePayload parse_remove(InPacket& recv);
    MovePayload parse_move(InPacket& recv);
    AttackPayload parse_attack(InPacket& recv);
    DamagePayload parse_damage(InPacket& recv);
    DragonPayload parse_dragon(InPacket& recv);
}
