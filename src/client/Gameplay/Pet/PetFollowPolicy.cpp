#include "PetFollowPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace jrc::pet_follow
{
    int32_t distance_between(Point<int16_t> first, Point<int16_t> second)
    {
        const int64_t dx = static_cast<int32_t>(first.x()) - second.x();
        const int64_t dy = static_cast<int32_t>(first.y()) - second.y();
        return static_cast<int32_t>(std::sqrt(
            static_cast<double>(dx * dx + dy * dy)));
    }

    bool made_progress(int32_t previous_distance, int32_t current_distance)
    {
        constexpr int32_t MINIMUM_PROGRESS = 4;
        return previous_distance <= 0 ||
            current_distance <= previous_distance - MINIMUM_PROGRESS;
    }

    bool outside_owner_leash(Point<int16_t> pet_position,
        Point<int16_t> owner_position)
    {
        const int32_t dx = static_cast<int32_t>(owner_position.x()) -
            pet_position.x();
        const int32_t dy = static_cast<int32_t>(owner_position.y()) -
            pet_position.y();
        const int64_t distance_sq = static_cast<int64_t>(dx) * dx +
            static_cast<int64_t>(dy) * dy;
        return distance_sq > static_cast<int64_t>(OWNER_LEASH_DISTANCE) *
            OWNER_LEASH_DISTANCE;
    }

    Point<int16_t> player_side_position(Point<int16_t> owner_position)
    {
        constexpr int32_t SIDE_OFFSET = 35;
        const int32_t x = std::clamp(
            static_cast<int32_t>(owner_position.x()) - SIDE_OFFSET,
            static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
            static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
        return { static_cast<int16_t>(x), owner_position.y() };
    }

    Action decide(const Inputs& inputs)
    {
        const int32_t dx = static_cast<int32_t>(inputs.target_position.x()) -
            inputs.pet_position.x();
        const int32_t target_dy = static_cast<int32_t>(inputs.target_position.y()) -
            inputs.pet_position.y();
        const int32_t start_distance = inputs.pursuing_loot ? 32 : 65;
        const int32_t stop_distance = inputs.pursuing_loot ? 14 : 34;

        if (outside_owner_leash(inputs.pet_position, inputs.owner_position))
        {
            return Action::WARP_TO_OWNER;
        }

        if (inputs.pursuing_loot &&
            (std::abs(target_dy) > PATH_HEIGHT_LIMIT ||
                inputs.failed_jumps >= MAX_FAILED_LOOT_JUMPS))
        {
            return Action::ABANDON_LOOT;
        }

        const int32_t threshold = inputs.moving ? stop_distance : start_distance;
        if (dx > threshold)
            return Action::MOVE_RIGHT;
        if (dx < -threshold)
            return Action::MOVE_LEFT;
        return Action::IDLE;
    }

    bool should_jump(const JumpInputs& inputs)
    {
        if (!inputs.grounded || !inputs.wall_blocked)
            return false;

        if (inputs.movement == Action::MOVE_LEFT)
            return inputs.end_x >= inputs.start_x;
        if (inputs.movement == Action::MOVE_RIGHT)
            return inputs.end_x <= inputs.start_x;
        return false;
    }

    JumpProgress advance_jump(bool was_airborne, bool grounded)
    {
        return { was_airborne || !grounded, was_airborne && grounded };
    }
}
