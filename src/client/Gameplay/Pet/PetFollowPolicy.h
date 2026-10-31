#pragma once

#include "../../Template/Point.h"
#include "../Physics/Physics.h"

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

    // Mirrors Player::get_walkforce / get_jumpforce, so pet ground movement
    // matches the owner exactly.
    struct MovementProfile
    {
        double walk_force = 0.16;
        double jump_force = 4.5;
    };

    constexpr MovementProfile BASE_MOVEMENT = {};

    // The drop icon center sits this far below its foothold.
    constexpr int32_t HEIGHT_TOLERANCE = 16;
    // Loot deeper below than this is not pursued; falling normally reaches it.
    constexpr int32_t FALL_HEIGHT_LIMIT = 240;

    constexpr int32_t OWNER_LEASH_DISTANCE = 300;
    // Chasing loot may stray 5x farther than the follow leash so the pet
    // can reach drops targeted at the expanded loot range.
    constexpr int32_t LOOT_LEASH_DISTANCE = 1500;
    constexpr uint8_t MAX_FAILED_LOOT_JUMPS = 3;

    // Apex height in pixels a takeoff speed of jump_force reaches under gravity.
    int32_t jump_reach_height(double jump_force, double gravity);
    // Ledge height the owner itself clears with one jump, plus the drop-center
    // tolerance. Loot above it is not pursued.
    int32_t pursuit_height_limit(const MovementProfile& profile);

    struct Inputs
    {
        Point<int16_t> pet_position;
        Point<int16_t> owner_position;
        Point<int16_t> target_position;
        bool pursuing_loot = false;
        bool moving = false;
        uint8_t failed_jumps = 0;
        int32_t height_limit = pursuit_height_limit(BASE_MOVEMENT);
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
        Point<int16_t> owner_position,
        int32_t limit = OWNER_LEASH_DISTANCE);
    Point<int16_t> player_side_position(Point<int16_t> owner_position);
    // Re-anchor a warped pet to its owner's foothold, so update_fh evaluates
    // the landing spot against the owner's platform, not the stale one.
    void anchor_after_warp(PhysicsObject& phobj, uint16_t owner_fhid);
}
