#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace jrc::character_preview
{
    std::optional<size_t> next_index(
        const std::vector<bool>& ready,
        size_t selected,
        size_t page,
        size_t page_size
    );
}
