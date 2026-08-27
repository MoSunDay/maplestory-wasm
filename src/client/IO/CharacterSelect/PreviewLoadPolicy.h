#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace jrc::character_preview
{
    enum class LoadState
    {
        PENDING,
        READY,
        FAILED
    };

    bool is_resolved(LoadState state);
    size_t resolved_count(const std::vector<LoadState>& states);
    bool should_announce_completion(
        const std::vector<LoadState>& states,
        bool completion_announced
    );

    std::optional<size_t> next_index(
        const std::vector<LoadState>& states,
        size_t selected,
        size_t page,
        size_t page_size
    );
}
