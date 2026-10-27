#pragma once

#include <cstddef>
#include <cstdint>

#include "../../Template/Point.h"

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
    // Horizontal half-window of the pet pickup sweep around the pet.
    constexpr int16_t BATCH_RANGE = 50;
    // A drop's icon center sits ~16px below its foothold and often lands a
    // platform step below the pet's feet; vertical reach must cover that
    // while staying inside the pursuit band (jump reach up, fall guard
    // down), so a parked pet is never locked out of adjacent loot.
    constexpr int32_t REACH_DROP_LIMIT = 96;
    // Pursuit that has not reached pickup range by this many frames can
    // never close the gap; give the drop up instead of standing frozen.
    constexpr uint16_t ABANDON_FRAMES = 240;

    bool should_attempt(uint16_t target_frames, uint8_t attempts,
        bool pet_in_range);
    bool attempts_exhausted(uint8_t attempts);
    // True when the pet is horizontally beside the target and the target is
    // no higher than `up_limit` above nor deeper than REACH_DROP_LIMIT below
    // the pet. Replaces the old euclidean 50px circle, which rejected drops
    // resting one ledge below the parked pet.
    bool in_reach(Point<int16_t> pet_position, Point<int16_t> target_position,
        int32_t up_limit);
    // Escape hatch so the loot state machine can never freeze: a target still
    // out of reach after ABANDON_FRAMES is given up by the caller.
    bool should_abandon(uint16_t target_frames, bool pet_in_range);
    uint8_t batch_count(size_t available);
}
