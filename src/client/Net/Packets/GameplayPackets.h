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
#include "MovementPacket.h"


namespace jrc
{
    /// Requests the server to warp the player to a different map.
    /// Opcode: CHANGE_MAP(38)
    class ChangeMapPacket : public OutPacket
    {
    public:
        ChangeMapPacket(bool died, int32_t targetid, const std::string& targetp, bool usewheel) : OutPacket(CHANGEMAP)
        {
            write_byte(died);
            write_int(targetid);
            write_string(targetp);
            skip(1);
            write_short(usewheel ? 1 : 0);
        }
    };


    /// Updates the player's position with the server.
    /// Opcode: MOVE_PLAYER(41)
    class MovePlayerPacket : public MovementPacket
    {
    public:
        MovePlayerPacket(const Movement& movement) : MovementPacket(MOVE_PLAYER)
        {
            skip(9);
            write_byte(1);
            writemovement(movement);
        }
    };

    /// Selects a map seat, or stops sitting when seat_id is -1.
    class CancelChairPacket : public OutPacket
    {
    public:
        explicit CancelChairPacket(int16_t seat_id = -1) : OutPacket(CANCEL_CHAIR)
        {
            write_short(seat_id);
        }
    };

    /// Requests sitting in an owned SETUP inventory chair.
    class UseChairPacket : public OutPacket
    {
    public:
        explicit UseChairPacket(int32_t item_id) : OutPacket(USE_CHAIR)
        {
            write_int(item_id);
        }
    };

    /// Reports the v83 client's natural HP/MP recovery tick.
    class HealOverTimePacket : public OutPacket
    {
    public:
        HealOverTimePacket(int16_t hp_gain, int16_t mp_gain)
            : OutPacket(HEAL_OVER_TIME)
        {
            // Cosmic consumes two legacy client integers before the gains.
            write_time();
            skip(4);
            write_short(hp_gain);
            write_short(mp_gain);
        }
    };

    /// Requests various party-related things.
    /// Opcode: PARTY_OPERATION(124)
    class PartyOperationPacket : public OutPacket
    {
    public:
        enum Operation : int8_t
        {
            CREATE      = 1,
            LEAVE       = 2,
            JOIN        = 3,
            INVITE      = 4,
            EXPEL       = 5,
            PASS_LEADER = 6
        };

    protected:
        PartyOperationPacket(Operation op) : OutPacket(PARTY_OPERATION)
        {
            write_byte(op);
        }
    };


    /// Creates a new party.
    /// Operation: CREATE(1)
    class CreatePartyPacket : public PartyOperationPacket
    {
    public:
        CreatePartyPacket() : PartyOperationPacket(CREATE) {}
    };


    /// Leaves a party
    /// Operation: LEAVE(2)
    class LeavePartyPacket : public PartyOperationPacket
    {
    public:
        LeavePartyPacket() : PartyOperationPacket(LEAVE) {}
    };


    /// Joins a party.
    /// Operation: JOIN(3)
    class JoinPartyPacket : public PartyOperationPacket
    {
    public:
        JoinPartyPacket(int32_t party_id) : PartyOperationPacket(JOIN)
        {
            write_int(party_id);
        }
    };


    /// Invites a player to a party.
    /// Operation: INVITE(4)
    class InviteToPartyPacket : public PartyOperationPacket
    {
    public:
        InviteToPartyPacket(const std::string& name) : PartyOperationPacket(INVITE)
        {
            write_string(name);
        }
    };


    /// Expels someone from a party.
    /// Operation: EXPEL(5)
    class ExpelFromPartyPacket : public PartyOperationPacket
    {
    public:
        ExpelFromPartyPacket(int32_t cid) : PartyOperationPacket(EXPEL)
        {
            write_int(cid);
        }
    };


    /// Passes party leadership to another character.
    /// Operation: PASS_LEADER(6)
    class ChangePartyLeaderPacket : public PartyOperationPacket
    {
    public:
        ChangePartyLeaderPacket(int32_t cid) : PartyOperationPacket(PASS_LEADER)
        {
            write_int(cid);
        }
    };

    /// Denies a pending party invitation.
    /// Opcode: DENY_PARTY_REQUEST(125)
    class DenyPartyInvitePacket : public OutPacket
    {
    public:
        DenyPartyInvitePacket(const std::string& inviter) : OutPacket(DENY_PARTY_REQUEST)
        {
            // v83 packet layout starts with an unused byte, followed by inviter name.
            write_byte(0);
            write_string(inviter);
        }
    };


    /// Updates a mob's position with the server.
    /// Opcode: MOVE_MONSTER(188)
    class MoveMobPacket : public MovementPacket
    {
    public:
        MoveMobPacket(int32_t oid, int16_t type, int8_t skillb, int8_t skill0, int8_t skill1,
            int8_t skill2, int8_t skill3, int8_t skill4, Point<int16_t> startpos,
            const Movement& movement) : MovementPacket(MOVE_MONSTER) {

            write_int(oid);
            write_short(type);
            write_byte(skillb);
            write_byte(skill0);
            write_byte(skill1);
            write_byte(skill2);
            write_byte(skill3);
            write_byte(skill4);

            skip(13);

            write_point(startpos);

            write_byte(1);
            writemovement(movement);
        }
    };


    /// Requests picking up an item.
    /// Opcode: PICKUP_ITEM(202)
    class PickupItemPacket : public OutPacket
    {
    public:
        PickupItemPacket(int32_t oid, Point<int16_t> position) : OutPacket(PICKUP_ITEM)
        {
            // Cosmic-based servers typically validate client tick/time here.
            write_time();
            write_byte(0);
            write_point(position);
            write_int(oid);
        }
    };

    /// Tells the server that we're no longer transitioning between maps,
    /// including initial login.
    /// Opcode: PLAYER_UPDATE(0xDF)
    class PlayerUpdatePacket : public OutPacket
    {
    public:
        PlayerUpdatePacket() : OutPacket(PLAYER_UPDATE) {}
    };
}
