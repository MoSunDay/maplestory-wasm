#pragma once

#include <cstddef>
#include <cstdint>

namespace jrc::pet_loot
{
    // The server locks a new drop for 400 ms; 55 fixed 8 ms updates include
    // a small scheduler margin before the first pickup request.
    constexpr uint16_t SERVER_DROP_LOCK_FRAMES = 55;
    constexpr uint16_t ATTEMPT_INTERVAL_FRAMES = 25;
    constexpr uint8_t MAX_ATTEMPTS = 3;

    // Pickup requests are swept in batches around the pet; the rest of a
    // cluster follows on the next attempt.
    constexpr uint8_t BATCH_LIMIT = 4;
    // Kept inside the linked server's 60px pet-to-drop validation.
    constexpr int16_t BATCH_RANGE = 50;

    bool should_attempt(uint16_t target_frames, uint8_t attempts,
        bool pet_in_range);
    bool attempts_exhausted(uint8_t attempts);
    uint8_t batch_count(size_t available);
}
