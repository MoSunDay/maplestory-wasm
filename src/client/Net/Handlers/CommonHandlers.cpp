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
#include "CommonHandlers.h"

#include "../Packets/CommonPackets.h"

#include <cstdint>
#include <iostream>
#include <set>

namespace jrc
{
    void PingHandler::handle(InPacket&) const
    {
        PongPacket().dispatch();
    }

    namespace
    {
        // File-level, once-per-opcode bookkeeping so a chatty server cannot
        // flood the log with announcements for deliberately dropped packets.
        std::set<uint16_t> announced_unsupported_opcodes;

        void announce_unsupported_once(uint16_t opcode, const char* name)
        {
            if (announced_unsupported_opcodes.insert(opcode).second)
            {
                std::cerr << "[PacketSwitch] Dropped unsupported packet, Opcode: "
                          << opcode << " (" << name << ")" << std::endl;
            }
        }
    }

    NullHandler::NullHandler(uint16_t opcode, const char* name)
        : opcode_(opcode), name_(name)
    {
    }

    void NullHandler::handle(InPacket&) const
    {
        if (name_ != nullptr)
        {
            announce_unsupported_once(opcode_, name_);
        }
    }
}
