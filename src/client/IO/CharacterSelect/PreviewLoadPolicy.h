#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace jrc::character_preview
{
    enum class LoadState
    {
        PENDING,
        READY,
        FAILED
    };

    struct LoadResult
    {
        LoadState state;
        std::string error;
    };

    bool is_resolved(LoadState state);
    LoadResult attempt_load(const std::function<void()>& loader);
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
