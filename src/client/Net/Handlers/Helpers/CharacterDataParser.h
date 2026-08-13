#pragma once
#include "../../InPacket.h"

namespace jrc::CharacterDataParser
{
    void parse_minigame_info(InPacket& recv);
    void parse_ring_info(InPacket& recv);
    void parse_new_year_info(InPacket& recv);
}
