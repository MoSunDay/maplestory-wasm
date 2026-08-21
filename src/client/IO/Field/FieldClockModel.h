#pragma once

#include <cstdint>
#include <string>

namespace jrc::field_clock
{
    enum class Mode
    {
        INACTIVE,
        WALL_CLOCK,
        COUNTDOWN
    };

    int32_t display_seconds(Mode mode, int32_t initial_seconds, int64_t elapsed_seconds);
    std::string format_seconds(int32_t seconds, bool always_show_hours);
}
