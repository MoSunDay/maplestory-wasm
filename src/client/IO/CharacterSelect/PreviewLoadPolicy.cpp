#include "PreviewLoadPolicy.h"

#include <algorithm>
#include <exception>

namespace jrc::character_preview
{
    bool is_resolved(LoadState state)
    {
        return state != LoadState::PENDING;
    }

    LoadResult attempt_load(const std::function<void()>& loader)
    {
        try
        {
            loader();
            return { LoadState::READY, "" };
        }
        catch (const std::exception& error)
        {
            return { LoadState::FAILED, error.what() };
        }
        catch (...)
        {
            return { LoadState::FAILED, "unknown exception" };
        }
    }

    size_t resolved_count(const std::vector<LoadState>& states)
    {
        return static_cast<size_t>(std::count_if(
            states.begin(),
            states.end(),
            is_resolved
        ));
    }

    bool should_announce_completion(
        const std::vector<LoadState>& states,
        bool completion_announced
    ) {
        return !completion_announced &&
            resolved_count(states) == states.size();
    }

    std::optional<size_t> next_index(
        const std::vector<LoadState>& states,
        size_t selected,
        size_t page,
        size_t page_size
    ) {
        if (selected < states.size() && !is_resolved(states[selected]))
        {
            return selected;
        }

        const bool page_offset_fits = page_size == 0 ||
            page <= states.size() / page_size;
        const size_t first = page_offset_fits
            ? std::min(page * page_size, states.size())
            : states.size();
        const size_t last = first + std::min(page_size, states.size() - first);
        for (size_t index = first; index < last; ++index)
        {
            if (!is_resolved(states[index]))
            {
                return index;
            }
        }

        for (size_t index = 0; index < states.size(); ++index)
        {
            if (!is_resolved(states[index]))
            {
                return index;
            }
        }

        return std::nullopt;
    }
}
