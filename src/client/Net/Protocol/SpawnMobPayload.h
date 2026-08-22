#pragma once

#include "../InPacket.h"

#include <cstdint>

namespace jrc::spawn_mob
{
    enum class Layout
    {
        STANDARD,
        MAKE_REAL
    };

    struct Payload
    {
        Point<int16_t> position;
        int8_t stance;
        uint16_t foothold;
        bool new_spawn;
        int8_t team;
        int8_t effect;
        int32_t parent_oid;
    };

    Payload parse_payload(InPacket& recv, Layout layout = Layout::STANDARD);
}
