#pragma once

#include "../PacketHandler.h"

namespace jrc
{
    class TradeHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };
}
