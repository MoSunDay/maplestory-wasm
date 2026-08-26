#include "PetFollowPolicy.h"

#include <cmath>

namespace jrc::pet_follow
{
    Action decide(const Inputs& inputs)
    {
        const int32_t dx = static_cast<int32_t>(inputs.target_position.x()) -
            inputs.pet_position.x();
        const int32_t dy = static_cast<int32_t>(inputs.target_position.y()) -
            inputs.pet_position.y();
        const int64_t distance_sq = static_cast<int64_t>(dx) * dx +
            static_cast<int64_t>(dy) * dy;

        const int32_t warp_distance = inputs.pursuing_loot ? 210 : 260;
        const int32_t warp_height = inputs.pursuing_loot ? 70 : 90;
        const int32_t start_distance = inputs.pursuing_loot ? 32 : 65;
        const int32_t stop_distance = inputs.pursuing_loot ? 14 : 34;

        if (std::abs(dy) > warp_height ||
            distance_sq > warp_distance * warp_distance ||
            (inputs.stalled_frames >= 90 && std::abs(dx) > stop_distance))
        {
            return Action::WARP;
        }

        const int32_t threshold = inputs.moving ? stop_distance : start_distance;
        if (dx > threshold)
            return Action::MOVE_RIGHT;
        if (dx < -threshold)
            return Action::MOVE_LEFT;
        return Action::IDLE;
    }
}
