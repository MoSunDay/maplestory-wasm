#pragma once

#include <cstdint>

namespace jrc::pet_loot
{
    // The server locks a new drop for 400 ms; 55 fixed 8 ms updates include
    // a small scheduler margin before the first pickup request.
    constexpr uint16_t SERVER_DROP_LOCK_FRAMES = 55;
    constexpr uint16_t ATTEMPT_INTERVAL_FRAMES = 25;
    constexpr uint8_t MAX_ATTEMPTS = 3;

    bool should_attempt(uint16_t target_frames, uint8_t attempts,
        bool pet_in_range);
    bool attempts_exhausted(uint8_t attempts);
}
