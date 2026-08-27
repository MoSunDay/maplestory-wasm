#include "CharlistTransition.h"

#include <algorithm>
#include <limits>

namespace jrc::charlist_transition
{
    namespace
    {
        Snapshot current;

        int32_t narrow_size(size_t value)
        {
            const size_t maximum = static_cast<size_t>(
                std::numeric_limits<int32_t>::max()
            );
            return static_cast<int32_t>(std::min(value, maximum));
        }
    }

    void reset(size_t remaining_bytes, size_t character_count)
    {
        current = {};
        current.phase = Phase::RECEIVED;
        current.remaining_bytes = narrow_size(remaining_bytes);
        current.preview_total = narrow_size(character_count);
    }

    void set_phase(Phase phase)
    {
        current.phase = phase;
    }

    void set_character(size_t index, size_t remaining_bytes)
    {
        current.character_index = narrow_size(index);
        current.equip_id = 0;
        current.remaining_bytes = narrow_size(remaining_bytes);
    }

    void set_equip(int32_t equip_id)
    {
        current.equip_id = equip_id;
    }

    void set_remaining(size_t remaining_bytes)
    {
        current.remaining_bytes = narrow_size(remaining_bytes);
    }

    void set_preview_progress(size_t ready, size_t total)
    {
        current.preview_ready = narrow_size(ready);
        current.preview_total = narrow_size(total);
    }

    Snapshot snapshot()
    {
        return current;
    }
}
