#pragma once

#include "SpawnPlayerHeader.h"

#include "../Login.h"

#include "../../Character/Look/PetSpawnEntry.h"
#include "../../Template/Point.h"

#include <cstdint>

namespace jrc::spawn_player
{
    struct Payload
    {
        Header header;
        LookEntry look;
        int16_t job = 0;
        int8_t stance = 0;
        Point<int16_t> position;
        int16_t combo_value = 0;
        PetSpawnEntry pets[3];
        uint8_t pet_count = 0;
    };

    /// Parse the body of SPAWN_PLAYER after the fixed spawn header.
    /// Consumes exactly the bytes written by the linked v83 server,
    /// including conditional foreign-buff, pet, shop and ring blocks.
    Payload parse_payload(InPacket& recv);
}
