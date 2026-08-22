#pragma once

#include "../../PacketHandler.h"

namespace jrc
{
    class BuddyListHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class PartyOperationHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class PartyValueHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class UpdatePartyMemberHpHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };
}
