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
#include "CharacterCreation/CreateCharPayload.h"

namespace jrc
{
    // Reserve a name for the character to be created.
    // Opcode: NAME_CHAR(21)
    class NameCharPacket : public OutPacket
    {
    public:
        NameCharPacket(const std::string& name) : OutPacket(NAME_CHAR)
        {
            write_string(name);
        }
    };

    // Requests creation of a character with the specified stats.
    // Opcode: CREATE_CHAR(22)
    class CreateCharPacket : public OutPacket
    {
    public:
        CreateCharPacket(const std::string& name, uint16_t job, int32_t face, int32_t hair,
            uint8_t hairc, uint8_t skin, int32_t top, int32_t bot, int32_t shoes,
            int32_t weapon, bool female) : OutPacket(CREATE_CHAR)
        {
            const std::vector<int8_t> payload = CharacterCreation::encode_create_char_payload({
                name,
                job,
                face,
                hair,
                hairc,
                skin,
                top,
                bot,
                shoes,
                weapon,
                static_cast<int8_t>(female)
            });
            for (int8_t byte : payload)
            {
                write_byte(byte);
            }
        }
    };
}
