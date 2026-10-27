#include "PetLootPolicy.h"

#include <algorithm>
#include <cstdlib>

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

    bool in_reach(Point<int16_t> pet_position, Point<int16_t> target_position,
        int32_t up_limit)
    {
        const int32_t dx = std::abs(static_cast<int32_t>(pet_position.x()) -
            target_position.x());
        // Y grows downward: positive dy means the target rests below the pet.
        const int32_t dy = static_cast<int32_t>(target_position.y()) -
            pet_position.y();
        return dx <= BATCH_RANGE && dy <= REACH_DROP_LIMIT && -dy <= up_limit;
    }

    bool should_abandon(uint16_t target_frames, bool pet_in_range)
    {
        return !pet_in_range && target_frames >= ABANDON_FRAMES;
    }
}
