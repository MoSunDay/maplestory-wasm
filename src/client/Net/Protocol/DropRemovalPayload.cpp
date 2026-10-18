#include "DropRemovalPayload.h"

#include "../InPacket.h"

namespace jrc::drop_removal
{
    namespace
    {
        void require_end(const InPacket& recv)
        {
            if (recv.available())
            {
                throw PacketError("Trailing drop removal payload");
            }
        }
    }

    Payload parse_payload(InPacket& recv)
    {
        const int8_t mode = recv.read_byte();
        const int32_t oid = recv.read_int();
        const Policy policy = policy_for_mode(mode);
        Payload payload{ mode, oid, 0, -1, policy.effect, policy.known_mode };

        if (!policy.known_mode)
        {
            recv.skip(recv.length());
            return payload;
        }

        if (policy.has_character_id)
        {
            payload.character_id = recv.read_int();
        }
        if (policy.has_pet_index)
        {
            payload.pet_index = recv.read_byte();
        }

        require_end(recv);
        return payload;
    }
}
