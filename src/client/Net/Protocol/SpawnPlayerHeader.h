#pragma once

#include "../InPacket.h"

#include <cstdint>
#include <string>

namespace jrc::spawn_player
{
    struct Header
    {
        int32_t cid;
        uint16_t level;
        std::string name;
    };

    Header parse_header(InPacket& recv);
}
