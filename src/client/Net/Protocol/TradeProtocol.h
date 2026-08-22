#pragma once

#include "../InPacket.h"

#include <cstdint>

namespace jrc::trade_protocol
{
    struct ItemSummary
    {
        int32_t item_id;
        int16_t quantity;
    };

    ItemSummary parse_item(InPacket& packet);
}
