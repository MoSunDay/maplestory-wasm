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
#include "../PacketHandler.h"

namespace jrc
{
    // Spawn a remote summon (puppet, bird, octopus, beholder, ...).
    // Opcode: SPAWN_SPECIAL_MAPOBJECT(175)
    struct SpawnSummonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Remove a remote summon, animated or not.
    // Opcode: REMOVE_SPECIAL_MAPOBJECT(176)
    struct RemoveSummonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Replay remote summon movement fragments.
    // Opcode: MOVE_SUMMON(177)
    struct MoveSummonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Show a remote summon attacking monsters.
    // Opcode: SUMMON_ATTACK(178)
    struct SummonAttackHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Show damage dealt to a remote summon.
    // Opcode: DAMAGE_SUMMON(179)
    struct DamageSummonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Spawn a priest/bishop dragon as a regular summon entity.
    // Opcode: SPAWN_DRAGON(181)
    struct SpawnDragonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Replay remote dragon movement fragments; the owner cid doubles as
    // the dragon's object id.
    // Opcode: MOVE_DRAGON(182)
    struct MoveDragonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };


    // Remove a remote dragon when its owner leaves the map or changes job.
    // Opcode: REMOVE_DRAGON(183)
    struct RemoveDragonHandler : PacketHandler
    {
        void handle(InPacket& recv) const override;
    };
}
