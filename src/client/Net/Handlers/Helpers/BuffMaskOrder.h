#pragma once

#include <cstdint>
#include <vector>

namespace jrc::buff_mask
{
    inline std::vector<uint64_t> ordered_flags(uint64_t mask)
    {
        std::vector<uint64_t> flags;
        for (uint8_t bit = 0; bit < 64; ++bit)
        {
            const uint64_t flag = uint64_t{1} << bit;
            if (mask & flag)
                flags.push_back(flag);
        }
        return flags;
    }

    template <typename Id, typename Mapping>
    struct Selection
    {
        std::vector<Id> ids;
        uint64_t unsupported = 0;
    };

    template <typename Id, typename Mapping>
    Selection<Id, Mapping> ordered_ids(uint64_t mask, const Mapping& mapping)
    {
        Selection<Id, Mapping> selection;
        for (uint64_t flag : ordered_flags(mask))
        {
            auto selected = mapping.end();
            for (auto candidate = mapping.begin(); candidate != mapping.end(); ++candidate)
            {
                if (candidate->second == flag
                    && (selected == mapping.end() || candidate->first < selected->first))
                {
                    selected = candidate;
                }
            }
            if (selected == mapping.end())
                selection.unsupported |= flag;
            else
                selection.ids.push_back(selected->first);
        }
        return selection;
    }
}
