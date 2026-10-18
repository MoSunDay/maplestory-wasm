#pragma once

#include "../OutPacket.h"

namespace jrc
{
    class CharacterInfoRequestPacket : public OutPacket
    {
    public:
        explicit CharacterInfoRequestPacket(int32_t character_id);
    };

    class GiveFamePacket : public OutPacket
    {
    public:
        GiveFamePacket(int32_t character_id, bool increase);
    };
}
