#pragma once

#include "../PacketHandler.h"
#include "../../IO/CashShop/CashShopModel.h"

namespace jrc
{
    class SetCashShopHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class QueryCashHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };

    class CashShopOperationHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;

        static CashLockerItem parse_locker_item(InPacket& recv, bool gift);
        static std::string error_message(uint8_t code);
    };

    class ChangeChannelHandler : public PacketHandler
    {
        void handle(InPacket& recv) const override;
    };
}
