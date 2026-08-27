#pragma once

#include "../../Gameplay/MapleMap/DropLifecycle/RemovalPolicy.h"

#include <cstdint>

namespace jrc
{
    class InPacket;
}

namespace jrc::drop_removal
{
    struct Payload
    {
        int8_t mode;
        int32_t oid;
        int32_t character_id;
        int8_t pet_index;
        Effect effect;
        bool known_mode;
    };

    Payload parse_payload(InPacket& recv);
}
