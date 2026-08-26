#pragma once

#include <cstdint>
#include <limits>

namespace jrc::damage_encoding
{
    struct Decoded
    {
        int32_t damage;
        bool critical;
    };

    constexpr int32_t encode(int32_t damage, bool critical)
    {
        const int32_t magnitude = damage > 0 ? damage : 0;
        return critical && magnitude > 0
            ? -std::numeric_limits<int32_t>::max() + magnitude - 1
            : magnitude;
    }

    constexpr Decoded decode(int32_t encoded)
    {
        return encoded < 0
            ? Decoded{ encoded + std::numeric_limits<int32_t>::max() + 1, true }
            : Decoded{ encoded, false };
    }
}
