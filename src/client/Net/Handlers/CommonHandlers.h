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
#include "../PacketHandler.h"

#include <cstdint>

namespace jrc
{
    class PingHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    // Drops a packet without parsing it. Registrations for opcodes that this
    // port deliberately does not support carry an (opcode, name) tag so the
    // first hit is announced once; untagged instances stay fully silent,
    // exactly as before.
    class NullHandler : public PacketHandler
    {
    public:
        NullHandler() = default;
        NullHandler(uint16_t opcode, const char* name);

        void handle(InPacket& recv) const override;

    private:
        uint16_t opcode_ = 0;
        const char* name_ = nullptr;
    };
}
