#pragma once

#include "CharacterInfoModel.h"

#include "../InPacket.h"

#include <cstdint>
#include <vector>

namespace jrc::character_info
{
    CharacterInfoPayload parse_character_info(InPacket& packet);
    FameResponse parse_fame_response(InPacket& packet);
    std::vector<int8_t> encode_character_info_request(int32_t character_id);
    std::vector<int8_t> encode_give_fame(int32_t character_id, bool increase);
    const char* fame_status_message(FameStatus status);
}
