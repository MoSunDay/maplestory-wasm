#pragma once

#include <cstdint>

namespace jrc::frame_timing
{
    constexpr int64_t MAX_CATCH_UP_US = 250000;

    constexpr int64_t bounded_elapsed_us(int64_t elapsed_us)
    {
        if (elapsed_us <= 0)
            return 0;

        return elapsed_us > MAX_CATCH_UP_US ? MAX_CATCH_UP_US : elapsed_us;
    }
}
