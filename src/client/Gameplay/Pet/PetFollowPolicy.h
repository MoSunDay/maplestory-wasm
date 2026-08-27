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
        WARP_TO_OWNER,
        ABANDON_LOOT
    };

    constexpr double GROUND_WALK_FORCE = 0.16;
    constexpr int32_t OWNER_LEASH_DISTANCE = 150;
    constexpr int32_t OWNER_LEASH_HEIGHT = 90;

    struct Inputs
    {
        Point<int16_t> pet_position;
        Point<int16_t> owner_position;
        Point<int16_t> target_position;
        bool pursuing_loot = false;
        bool moving = false;
        uint16_t stalled_frames = 0;
    };

    Action decide(const Inputs& inputs);
    bool made_progress(int32_t previous_distance, int32_t current_distance);
    bool outside_owner_leash(Point<int16_t> pet_position,
        Point<int16_t> owner_position);
    Point<int16_t> player_side_position(Point<int16_t> owner_position);
}
