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
#include "PacketError.h"

#include "../Template/Point.h"

#include <cstdint>
#include <type_traits>


namespace jrc
{
    /// A packet received from the server.
    /// Contains reading functions.
    class InPacket
    {
    public:
        /// Construct a packet from an array of bytes.
        InPacket(const int8_t* bytes, size_t length);

        /// Check if there are more bytes available.
        bool available() const;
        /// Return the remaining length in bytes.
        size_t length() const;
        /// Skip a number of bytes (by increasing the offset).
        void skip(size_t count);

        /// Read a byte and check if it is 1.
        bool read_bool();
        /// Read a byte.
        int8_t read_byte();
        /// Read a short.
        int16_t read_short();
        /// Read a int.
        int32_t read_int();
        /// Read a long.
        int64_t read_long();

        /// Read a point.
        Point<int16_t> read_point();

        /// Read a string.
        std::string read_string();
        /// Read a fixed-length string.
        std::string read_padded_string(uint16_t length);
        /// Read a UTF-8 string padded to a fixed number of UTF-16 code units.
        std::string read_padded_utf8_string(uint16_t utf16_length);

        /// Inspect a byte and check if it is 1. Does not advance the buffer position.
        bool inspect_bool();
        /// Inspect a byte. Does not advance the buffer position.
        int8_t inspect_byte();
        /// Inspect a short. Does not advance the buffer position.
        int16_t inspect_short();
        /// Inspect an int. Does not advance the buffer position.
        int32_t inspect_int();
        /// Inspect a long. Does not advance the buffer position.
        int64_t inspect_long();

    private:
        template <typename T>
        /// Read a number and advance the buffer position.
        T read()
        {
            const size_t count = sizeof(T) / sizeof(int8_t);
            if (count > length())
            {
                throw PacketError("Stack underflow at " + std::to_string(pos));
            }

            using Unsigned = typename std::make_unsigned<T>::type;
            Unsigned all = 0;
            for (size_t i = 0; i < count; ++i)
            {
                const auto value = static_cast<Unsigned>(
                    static_cast<uint8_t>(bytes[pos + i])
                );
                all |= value << (8 * i);
            }
            pos += count;

            return static_cast<T>(all);
        }

        template <typename T>
        /// Read without advancing the buffer position.
        T inspect()
        {
            size_t before = pos;
            T value = read<T>();
            pos = before;
            return value;
        }

        const int8_t* bytes;
        size_t top;
        size_t pos;
    };
}
