#pragma once

#include "../PacketHandler.h"

namespace jrc
{
    class CharacterInfoHandler : public PacketHandler
    {
        void handle(InPacket& packet) const override;
    };

    class FameResponseHandler : public PacketHandler
    {
        void handle(InPacket& packet) const override;
    };
}
