#include "PreviewLoadPolicy.h"

#include <algorithm>

namespace jrc::character_preview
{
    std::optional<size_t> next_index(
        const std::vector<bool>& ready,
        size_t selected,
        size_t page,
        size_t page_size
    ) {
        if (selected < ready.size() && !ready[selected])
        {
            return selected;
        }

        const bool page_offset_fits = page_size == 0 ||
            page <= ready.size() / page_size;
        const size_t first = page_offset_fits
            ? std::min(page * page_size, ready.size())
            : ready.size();
        const size_t last = first + std::min(page_size, ready.size() - first);
        for (size_t index = first; index < last; ++index)
        {
            if (!ready[index])
            {
                return index;
            }
        }

        for (size_t index = 0; index < ready.size(); ++index)
        {
            if (!ready[index])
            {
                return index;
            }
        }

        return std::nullopt;
    }
}
