#include "CharacterInfoPackets.h"

#include "CharacterInfoProtocol.h"

namespace jrc
{
    CharacterInfoRequestPacket::CharacterInfoRequestPacket(int32_t character_id)
        : OutPacket(CHAR_INFO_REQUEST)
    {
        write_bytes(character_info::encode_character_info_request(character_id));
    }

    GiveFamePacket::GiveFamePacket(int32_t character_id, bool increase)
        : OutPacket(GIVE_FAME)
    {
        write_bytes(character_info::encode_give_fame(character_id, increase));
    }
}
