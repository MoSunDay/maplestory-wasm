#include "PetLootPolicy.h"

namespace jrc::pet_loot
{
    bool should_attempt(uint16_t target_frames, uint8_t attempts,
        bool pet_in_range)
    {
        if (!pet_in_range || attempts >= MAX_ATTEMPTS)
            return false;

        uint16_t next_attempt = static_cast<uint16_t>(SERVER_DROP_LOCK_FRAMES +
            attempts * ATTEMPT_INTERVAL_FRAMES);
        return target_frames >= next_attempt;
    }

    bool attempts_exhausted(uint8_t attempts)
    {
        return attempts >= MAX_ATTEMPTS;
    }
}
