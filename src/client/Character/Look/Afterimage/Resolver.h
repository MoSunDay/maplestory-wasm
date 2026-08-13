#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

namespace jrc::afterimage
{
    // Later weapons can outlevel the last artwork tier. Select only from
    // tiers that contain the requested stance so another motion is never
    // substituted to hide incomplete data.
    inline std::optional<int16_t> select_level_bucket(
        int16_t requested,
        const std::vector<int16_t>& available
    )
    {
        if (available.empty())
        {
            return std::nullopt;
        }

        std::vector<int16_t> tiers = available;
        std::sort(tiers.begin(), tiers.end());
        tiers.erase(std::unique(tiers.begin(), tiers.end()), tiers.end());

        auto upper = std::upper_bound(tiers.begin(), tiers.end(), requested);
        if (upper == tiers.begin())
        {
            return tiers.front();
        }

        return *std::prev(upper);
    }
}
