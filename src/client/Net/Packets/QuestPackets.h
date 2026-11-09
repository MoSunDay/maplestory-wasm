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

namespace jrc
{
    // Quest interaction requests. Opcode: QUEST_ACTION(107).
    //
    // Server contract (link_repos/MapleStory-Server
    // src/net/server/channel/handlers/QuestActionHandler.java): the payload
    // is [byte action][short questid]; for actions 1/2/4/5 the reader then
    // consumes [int npcid] and, if at least 4 bytes are still available,
    // [short x][short y] for the isNpcNearby check (x before y, both after
    // the npcid). Action 2 additionally reads a trailing [short selection]
    // when at least 2 bytes remain; these packets omit it, so the server
    // takes the no-selection completion branch.

    // Requests starting a quest. Opcode: QUEST_ACTION(107)
    class QuestStartPacket : public OutPacket
    {
    public:
        QuestStartPacket(int16_t quest_id, int32_t npc_id,
            Point<int16_t> position) : OutPacket(QUEST_ACTION)
        {
            write_byte(START);
            write_short(quest_id);
            write_int(npc_id);
            write_short(position.x());
            write_short(position.y());
        }

    private:
        enum Mode : int8_t
        {
            START = 1
        };
    };

    // Requests completing a quest (no selection). Opcode: QUEST_ACTION(107)
    class QuestCompletePacket : public OutPacket
    {
    public:
        QuestCompletePacket(int16_t quest_id, int32_t npc_id,
            Point<int16_t> position) : OutPacket(QUEST_ACTION)
        {
            write_byte(COMPLETE);
            write_short(quest_id);
            write_int(npc_id);
            write_short(position.x());
            write_short(position.y());
        }

    private:
        enum Mode : int8_t
        {
            COMPLETE = 2
        };
    };

    // Requests starting a scripted quest (Check.img/<qid>/0/startscript).
    // Opcode: QUEST_ACTION(107)
    class QuestScriptStartPacket : public OutPacket
    {
    public:
        QuestScriptStartPacket(int16_t quest_id, int32_t npc_id,
            Point<int16_t> position) : OutPacket(QUEST_ACTION)
        {
            write_byte(SCRIPT_START);
            write_short(quest_id);
            write_int(npc_id);
            write_short(position.x());
            write_short(position.y());
        }

    private:
        enum Mode : int8_t
        {
            SCRIPT_START = 4
        };
    };

    // Requests completing a scripted quest (Check.img/<qid>/1/endscript).
    // Opcode: QUEST_ACTION(107)
    class QuestScriptEndPacket : public OutPacket
    {
    public:
        QuestScriptEndPacket(int16_t quest_id, int32_t npc_id,
            Point<int16_t> position) : OutPacket(QUEST_ACTION)
        {
            write_byte(SCRIPT_END);
            write_short(quest_id);
            write_int(npc_id);
            write_short(position.x());
            write_short(position.y());
        }

    private:
        enum Mode : int8_t
        {
            SCRIPT_END = 5
        };
    };

    // Packet which forfeits a quest. Opcode: QUEST_ACTION(107)
    class QuestForfeitPacket : public OutPacket
    {
    public:
        explicit QuestForfeitPacket(int16_t quest_id) : OutPacket(QUEST_ACTION)
        {
            write_byte(FORFEIT);
            write_short(quest_id);
        }

    private:
        enum Mode : int8_t
        {
            FORFEIT = 3
        };
    };
}
