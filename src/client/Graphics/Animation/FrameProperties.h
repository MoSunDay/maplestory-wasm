#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace jrc::animation
{
    inline std::pair<uint8_t, uint8_t> opacity_endpoints(
        std::optional<uint8_t> start,
        std::optional<uint8_t> end
    ) {
        if (start && end) return { *start, *end };
        if (start) return { *start, *start };
        if (end) return { 255, *end };
        return { 255, 255 };
    }
}
