#include "SpawnMobPayload.h"

#include <cstdint>
#include <string>

namespace jrc::spawn_mob
{
    namespace
    {
        void require_byte(InPacket& recv, int8_t expected, const char* field)
        {
            const int8_t actual = recv.read_byte();
            if (actual != expected)
            {
                throw PacketError("Unexpected " + std::string(field));
            }
        }

        void require_short(InPacket& recv, int16_t expected, const char* field)
        {
            const int16_t actual = recv.read_short();
            if (actual != expected)
            {
                throw PacketError("Unexpected " + std::string(field));
            }
        }

        void require_int(InPacket& recv, int32_t expected, const char* field)
        {
            const int32_t actual = recv.read_int();
            if (actual != expected)
            {
                throw PacketError("Unexpected " + std::string(field));
            }
        }

        void require_end(const InPacket& recv)
        {
            if (recv.available())
            {
                throw PacketError("Trailing mob spawn payload");
            }
        }

        void parse_fixed_state(InPacket& recv, bool make_real)
        {
            for (uint8_t i = 0; i < 15; ++i)
            {
                require_byte(recv, 0, "mob spawn state padding");
            }

            require_byte(
                recv,
                static_cast<int8_t>(0x88),
                "mob spawn state marker"
            );

            const uint8_t trailing_padding = make_real ? 12 : 6;
            for (uint8_t i = 0; i < trailing_padding; ++i)
            {
                require_byte(recv, 0, "mob spawn state padding");
            }
        }

        int32_t make_int(int8_t byte0, int8_t byte1, int8_t byte2, int8_t byte3)
        {
            const auto part0 = static_cast<uint32_t>(static_cast<uint8_t>(byte0));
            const auto part1 = static_cast<uint32_t>(static_cast<uint8_t>(byte1)) << 8;
            const auto part2 = static_cast<uint32_t>(static_cast<uint8_t>(byte2)) << 16;
            const auto part3 = static_cast<uint32_t>(static_cast<uint8_t>(byte3)) << 24;
            return static_cast<int32_t>(part0 | part1 | part2 | part3);
        }

        bool is_regular_marker(int8_t value)
        {
            return value == -2 || value == -1;
        }

        int8_t parse_tail(InPacket& recv, Payload& payload)
        {
            if (recv.length() == 7 && recv.inspect_short() == -2)
            {
                recv.read_short(); // Fake-mob marker.
                payload.new_spawn = true;
            }
            else
            {
                const int8_t spawn_code = recv.read_byte();
                if (is_regular_marker(spawn_code))
                {
                    payload.new_spawn = spawn_code == -2;
                }
                else if (spawn_code == -3)
                {
                    payload.parent_oid = recv.read_int();
                }
                else if (spawn_code > 0)
                {
                    payload.effect = spawn_code;

                    const int8_t byte0 = recv.read_byte();
                    const int8_t byte1 = recv.read_byte();
                    const int8_t byte2 = recv.read_byte();
                    const int8_t next = recv.inspect_byte();
                    const bool parent_effect =
                        byte0 != 0 || byte1 != 0 || byte2 != 0 ||
                        (spawn_code == 15
                            ? next != 0 && recv.inspect_short() != -2
                            : !is_regular_marker(next));

                    if (parent_effect)
                    {
                        payload.parent_oid = make_int(
                            byte0, byte1, byte2, recv.read_byte()
                        );
                    }
                    else
                    {
                        const bool fake =
                            recv.length() == 7 && recv.inspect_short() == -2;
                        if (spawn_code == 15 && !fake)
                        {
                            require_byte(recv, 0, "effect 15 padding");
                        }

                        if (fake)
                        {
                            recv.read_short();
                            payload.new_spawn = true;
                        }
                        else
                        {
                            const int8_t marker = recv.read_byte();
                            if (!is_regular_marker(marker))
                            {
                                throw PacketError("Unexpected mob spawn marker");
                            }
                            payload.new_spawn = marker == -2;
                        }
                    }
                }
                else
                {
                    throw PacketError("Unexpected mob spawn effect");
                }
            }

            const int8_t team = recv.read_byte();
            require_int(recv, 0, "mob item effect");
            require_end(recv);
            return team;
        }
    }

    Payload parse_payload(InPacket& recv, Layout layout)
    {
        const bool make_real = layout == Layout::MAKE_REAL;
        parse_fixed_state(recv, make_real);

        Payload payload{
            recv.read_point(),
            recv.read_byte(),
            0,
            false,
            -1,
            0,
            0
        };

        require_short(recv, 0, "origin foothold");
        payload.foothold = static_cast<uint16_t>(recv.read_short());

        if (make_real)
        {
            require_short(recv, -1, "make-real marker");
            require_int(recv, 0, "mob item effect");
            require_end(recv);
            return payload;
        }

        payload.team = parse_tail(recv, payload);
        return payload;
    }
}
