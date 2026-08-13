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
#include "InPacket.h"


namespace jrc
{
    InPacket::InPacket(const int8_t* recv, size_t length)
    {
        bytes = recv;
        top   = length;
        pos   = 0;
    }

    bool InPacket::available() const
    {
        return length() > 0;
    }

    size_t InPacket::length() const
    {
        return top - pos;
    }

    void InPacket::skip(size_t count)
    {
        if (count > length())
        {
            throw PacketError("Stack underflow at " + std::to_string(pos));
        }

        pos += count;
    }

    bool InPacket::read_bool()
    {
        return read_byte() == 1;
    }

    int8_t InPacket::read_byte()
    {
        return read<int8_t>();
    }

    int16_t InPacket::read_short()
    {
        return read<int16_t>();
    }

    int32_t InPacket::read_int()
    {
        return read<int32_t>();
    }

    int64_t InPacket::read_long()
    {
        return read<int64_t>();
    }

    Point<int16_t> InPacket::read_point()
    {
        auto x = read<int16_t>();
        auto y = read<int16_t>();
        return { x, y };
    }

    std::string InPacket::read_string()
    {
        auto length = read<uint16_t>();
        return read_padded_string(length);
    }

    std::string InPacket::read_padded_string(uint16_t count)
    {
        std::string ret;

        for (int16_t i = 0; i < count; ++i)
        {
            char letter = read_byte();
            if (letter != '\0')
            {
                ret.push_back(letter);
            }
        }

        return ret;
    }

    std::string InPacket::read_padded_utf8_string(uint16_t utf16_length)
    {
        std::string result;
        uint16_t utf16_units = 0;

        while (utf16_units < utf16_length)
        {
            const auto first = static_cast<uint8_t>(read_byte());
            if (first == 0)
            {
                ++utf16_units;
                continue;
            }

            uint8_t byte_count = 0;
            uint8_t unit_count = 1;
            if (first <= 0x7F)
            {
                byte_count = 1;
            }
            else if (first >= 0xC2 && first <= 0xDF)
            {
                byte_count = 2;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                byte_count = 3;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                byte_count = 4;
                unit_count = 2;
            }
            else
            {
                throw PacketError("Malformed UTF-8 fixed string");
            }

            if (utf16_units + unit_count > utf16_length)
            {
                throw PacketError("UTF-8 character exceeds fixed string length");
            }

            result.push_back(static_cast<char>(first));
            for (uint8_t i = 1; i < byte_count; ++i)
            {
                const auto continuation = static_cast<uint8_t>(read_byte());
                const bool is_continuation =
                    continuation >= 0x80 && continuation <= 0xBF;
                const bool valid_lower_bound =
                    (i != 1) || (first != 0xE0) || continuation >= 0xA0;
                const bool valid_surrogate_bound =
                    (i != 1) || (first != 0xED) || continuation <= 0x9F;
                const bool valid_plane_lower_bound =
                    (i != 1) || (first != 0xF0) || continuation >= 0x90;
                const bool valid_plane_upper_bound =
                    (i != 1) || (first != 0xF4) || continuation <= 0x8F;
                if (!is_continuation || !valid_lower_bound ||
                    !valid_surrogate_bound || !valid_plane_lower_bound ||
                    !valid_plane_upper_bound)
                {
                    throw PacketError("Malformed UTF-8 fixed string");
                }
                result.push_back(static_cast<char>(continuation));
            }

            utf16_units += unit_count;
        }

        return result;
    }

    bool InPacket::inspect_bool()
    {
        return inspect_byte() == 1;
    }

    int8_t InPacket::inspect_byte()
    {
        return inspect<int8_t>();
    }

    int16_t InPacket::inspect_short()
    {
        return inspect<int16_t>();
    }

    int32_t InPacket::inspect_int()
    {
        return inspect<int32_t>();
    }

    int64_t InPacket::inspect_long()
    {
        return inspect<int64_t>();
    }
}
