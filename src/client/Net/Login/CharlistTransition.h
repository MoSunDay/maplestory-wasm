#pragma once

#include <cstddef>
#include <cstdint>

namespace jrc::charlist_transition
{
    enum class Phase : int32_t
    {
        IDLE = 0,
        RECEIVED = 1,
        PARSING = 2,
        PARSED = 3,
        UI_MOUNTING = 4,
        UI_MOUNTED = 5,
        STATIC_UI_LOADING = 6,
        PREVIEW_LOADING = 7,
        READY = 8,
        FAILED_PARSE = 9,
        FAILED_UI = 10
    };

    struct Snapshot
    {
        Phase phase = Phase::IDLE;
        int32_t character_index = -1;
        int32_t equip_id = 0;
        int32_t remaining_bytes = 0;
        int32_t preview_ready = 0;
        int32_t preview_total = 0;
    };

    void reset(size_t remaining_bytes, size_t character_count);
    void set_phase(Phase phase);
    void set_character(size_t index, size_t remaining_bytes);
    void set_equip(int32_t equip_id);
    void set_remaining(size_t remaining_bytes);
    void set_preview_progress(size_t ready, size_t total);
    Snapshot snapshot();
}
