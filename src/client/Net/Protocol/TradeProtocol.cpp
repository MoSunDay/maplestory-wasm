#include "TradeProtocol.h"

#include "../../Character/EquipStat.h"

namespace jrc::trade_protocol
{
    ItemSummary parse_item(InPacket& packet)
    {
        int8_t item_type = packet.read_byte();
        int32_t item_id = packet.read_int();
        bool cash = packet.read_bool();
        if (cash)
            packet.read_long();
        packet.read_long();

        if (item_id >= 5000000 && item_id <= 5000102)
        {
            packet.read_padded_string(13);
            packet.skip(22);
            return { item_id, 1 };
        }

        if (item_type == 1 || item_id / 1000000 == 1)
        {
            packet.skip(2 + Equipstat::LENGTH * 2);
            packet.read_string();
            packet.skip(2);
            packet.skip(cash ? 10 : 18);
            packet.skip(12);
            return { item_id, 1 };
        }

        int16_t quantity = packet.read_short();
        packet.read_string();
        packet.skip(2);
        if (item_id / 10000 == 207 || item_id / 10000 == 233)
            packet.skip(8);
        return { item_id, quantity };
    }
}
