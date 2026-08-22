#pragma once

#include "../OutPacket.h"
#include "../../Character/Inventory/InventoryType.h"

namespace jrc
{
    class CreateTradePacket : public OutPacket
    {
    public:
        CreateTradePacket();
    };

    class InviteTradePacket : public OutPacket
    {
    public:
        explicit InviteTradePacket(int32_t character_id);
    };

    class TradeActionPacket : public OutPacket
    {
    public:
        enum Action : int8_t { DECLINE = 3, VISIT = 4, EXIT = 10, CONFIRM = 17 };
        explicit TradeActionPacket(Action action);
    };

    class TradeChatPacket : public OutPacket
    {
    public:
        explicit TradeChatPacket(const std::string& message);
    };

    class TradeItemPacket : public OutPacket
    {
    public:
        TradeItemPacket(InventoryType::Id type, int16_t source,
            int16_t quantity, int8_t target);
    };

    class TradeMesoPacket : public OutPacket
    {
    public:
        explicit TradeMesoPacket(int32_t amount);
    };
}
