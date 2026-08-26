#pragma once

#include "../../Template/Point.h"

#include <cstdint>

namespace jrc::pet_follow
{
    enum class Action : uint8_t
    {
        IDLE,
        MOVE_LEFT,
        MOVE_RIGHT,
        WARP
    };

    struct Inputs
    {
        Point<int16_t> pet_position;
        Point<int16_t> target_position;
        bool pursuing_loot = false;
        bool moving = false;
        uint16_t stalled_frames = 0;
    };

    Action decide(const Inputs& inputs);
    bool made_progress(int32_t previous_distance, int32_t current_distance);
}
