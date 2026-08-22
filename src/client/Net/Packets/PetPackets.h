#pragma once

#include "MovementPacket.h"

namespace jrc
{
    class SpawnPetPacket : public OutPacket
    {
    public:
        explicit SpawnPetPacket(int8_t cash_slot, bool lead = true)
            : OutPacket(SPAWN_PET)
        {
            write_time();
            write_byte(cash_slot);
            write_byte(0);
            write_byte(lead ? 1 : 0);
        }
    };

    class MovePetPacket : public MovementPacket
    {
    public:
        MovePetPacket(int32_t pet_unique_id, const Movement& movement)
            : MovementPacket(MOVE_PET)
        {
            write_int(pet_unique_id);
            skip(8);
            write_byte(1);
            writemovement(movement);
        }
    };

    class PetLootPacket : public OutPacket
    {
    public:
        PetLootPacket(int32_t pet_unique_id, int32_t drop_oid)
            : OutPacket(PET_LOOT)
        {
            write_int(pet_unique_id);
            skip(13);
            write_int(drop_oid);
        }
    };

    class PetAutoPotPacket : public OutPacket
    {
    public:
        PetAutoPotPacket(uint8_t pet_index, int64_t pet_unique_id,
            int16_t use_slot, int32_t item_id)
            : OutPacket(PET_AUTO_POT)
        {
            write_byte(static_cast<int8_t>(pet_index));
            write_long(pet_unique_id);
            write_time();
            write_short(use_slot);
            write_int(item_id);
        }
    };
}
