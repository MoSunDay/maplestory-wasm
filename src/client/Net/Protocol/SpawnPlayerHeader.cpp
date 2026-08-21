#include "SpawnPlayerHeader.h"

namespace jrc::spawn_player
{
    Header parse_header(InPacket& recv)
    {
        return {
            recv.read_int(),
            static_cast<uint16_t>(recv.read_short()),
            recv.read_string()
        };
    }
}
