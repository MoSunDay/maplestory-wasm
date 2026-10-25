#include "PetLootPolicy.h"

#include <algorithm>

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

    uint8_t batch_count(size_t available)
    {
        return static_cast<uint8_t>(
            std::min(available, static_cast<size_t>(BATCH_LIMIT)));
    }
}
