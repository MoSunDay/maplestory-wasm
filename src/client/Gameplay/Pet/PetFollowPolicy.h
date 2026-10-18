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
    constexpr double JUMP_HORIZONTAL_SPEED = 1.25;
    constexpr int32_t OWNER_LEASH_DISTANCE = 300;
    constexpr int32_t PATH_HEIGHT_LIMIT = 150;
    constexpr uint8_t MAX_FAILED_LOOT_JUMPS = 3;

    struct Inputs
    {
        Point<int16_t> pet_position;
        Point<int16_t> owner_position;
        Point<int16_t> target_position;
        bool pursuing_loot = false;
        bool moving = false;
        uint8_t failed_jumps = 0;
    };

    struct JumpInputs
    {
        Action movement = Action::IDLE;
        bool grounded = false;
        bool wall_blocked = false;
        double start_x = 0.0;
        double end_x = 0.0;
    };

    struct JumpProgress
    {
        bool airborne = false;
        bool landed = false;
    };

    Action decide(const Inputs& inputs);
    bool should_jump(const JumpInputs& inputs);
    JumpProgress advance_jump(bool was_airborne, bool grounded);
    int32_t distance_between(Point<int16_t> first, Point<int16_t> second);
    bool made_progress(int32_t previous_distance, int32_t current_distance);
    bool outside_owner_leash(Point<int16_t> pet_position,
        Point<int16_t> owner_position);
    Point<int16_t> player_side_position(Point<int16_t> owner_position);
}
