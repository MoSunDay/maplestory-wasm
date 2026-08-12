#pragma once

#include <cstdint>
#include <vector>

namespace jrc::foreign_buff
{
    inline std::vector<uint64_t> ordered_flags(uint64_t mask)
    {
        std::vector<uint64_t> flags;
        for (uint8_t bit = 0; bit < 64; ++bit)
        {
            uint64_t flag = uint64_t{1} << bit;
            if (mask & flag)
                flags.push_back(flag);
        }
        return flags;
    }
}
