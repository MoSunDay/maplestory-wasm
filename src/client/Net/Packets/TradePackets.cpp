#include "TradePackets.h"

namespace jrc
{
    CreateTradePacket::CreateTradePacket()
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(0);
        write_byte(3);
    }

    InviteTradePacket::InviteTradePacket(int32_t character_id)
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(2);
        write_int(character_id);
    }

    TradeActionPacket::TradeActionPacket(Action action)
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(action);
    }

    TradeChatPacket::TradeChatPacket(const std::string& message)
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(6);
        write_string(message);
    }

    TradeItemPacket::TradeItemPacket(InventoryType::Id type, int16_t source,
        int16_t quantity, int8_t target)
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(15);
        write_byte(static_cast<int8_t>(type));
        write_short(source);
        write_short(quantity);
        write_byte(target);
    }

    TradeMesoPacket::TradeMesoPacket(int32_t amount)
        : OutPacket(OutPacket::PLAYER_INTERACTION)
    {
        write_byte(16);
        write_int(amount);
    }
}
