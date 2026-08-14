#include "CharacterDataParser.h"

namespace jrc::CharacterDataParser
{
    namespace
    {
        int16_t read_count(InPacket& recv, const char* field)
        {
            const auto count = recv.read_short();
            if (count < 0)
            {
                throw PacketError(std::string("Negative ") + field + " count");
            }
            return count;
        }
    }

    void parse_minigame_info(InPacket& recv)
    {
        const auto count = read_count(recv, "minigame");
        if (count != 0)
        {
            throw PacketError("Unsupported non-empty minigame info");
        }
    }

    void parse_ring_info(InPacket& recv)
    {
        const auto crush_count = read_count(recv, "crush ring");
        for (int16_t i = 0; i < crush_count; ++i)
        {
            recv.read_int();
            recv.read_padded_string(13);
            recv.read_int();
            recv.read_int();
            recv.read_int();
            recv.read_int();
        }

        const auto friendship_count = read_count(recv, "friendship ring");
        for (int16_t i = 0; i < friendship_count; ++i)
        {
            recv.read_int();
            recv.read_padded_string(13);
            recv.read_int();
            recv.read_int();
            recv.read_int();
            recv.read_int();
            recv.read_int();
        }

        const auto marriage_count = read_count(recv, "marriage ring");
        for (int16_t i = 0; i < marriage_count; ++i)
        {
            recv.read_int();
            recv.read_int();
            recv.read_int();
            recv.read_short();
            recv.read_int();
            recv.read_int();
            recv.read_padded_string(13);
            recv.read_padded_string(13);
        }
    }

    void parse_new_year_info(InPacket& recv)
    {
        const auto count = read_count(recv, "new year card");
        for (int16_t i = 0; i < count; ++i)
        {
            recv.read_int();    // card id
            recv.read_int();    // sender id
            recv.read_string(); // sender name
            recv.read_bool();   // sender discarded
            recv.read_long();   // sent at
            recv.read_int();    // receiver id
            recv.read_string(); // receiver name
            recv.read_bool();   // receiver discarded
            recv.read_bool();   // receiver received
            recv.read_long();   // received at
            recv.read_string(); // message
        }
    }
}
