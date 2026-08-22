#pragma once

#include "../../Template/Point.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace jrc
{
    struct PlayerTarget
    {
        int32_t id = 0;
        std::string name;
        uint16_t level = 0;
        Point<int16_t> position;
    };

    std::optional<PlayerTarget> pick_player_target(
        const std::vector<PlayerTarget>& candidates,
        Point<int16_t> cursor,
        Point<int16_t> view_position
    );
}
