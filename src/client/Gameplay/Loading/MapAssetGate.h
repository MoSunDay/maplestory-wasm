#pragma once

#include <cstddef>
#include <cstdint>

namespace jrc::map_asset_gate
{
    constexpr uint64_t STABLE_WINDOW_MS = 250;
    constexpr uint64_t STALL_TIMEOUT_MS = 60000;

    struct Progress
    {
        size_t resident = 0;
        size_t total = 0;
        size_t prepared = 0;
        size_t required = 0;

        constexpr bool ready() const
        {
            return resident == total && prepared == required;
        }
    };

    struct State
    {
        uint64_t revision = 0;
        uint64_t stable_since_ms = 0;
        uint64_t last_progress_ms = 0;
        size_t last_resident = 0;
        size_t last_total = 0;
        size_t last_prepared = 0;
        size_t last_required = 0;
        bool failed = false;
    };

    struct Step
    {
        State state;
        bool progress_changed = false;
        bool ready = false;
        bool stalled = false;
    };

    constexpr State begin(uint64_t now_ms, uint64_t revision)
    {
        State state;
        state.revision = revision;
        state.stable_since_ms = now_ms;
        state.last_progress_ms = now_ms;
        return state;
    }

    constexpr Step advance(State state, Progress progress, uint64_t revision, uint64_t now_ms)
    {
        Step step;
        step.state = state;
        if (revision != state.revision)
        {
            step.state.revision = revision;
            step.state.stable_since_ms = now_ms;
        }

        step.progress_changed = progress.resident != state.last_resident ||
            progress.total != state.last_total ||
            progress.prepared != state.last_prepared ||
            progress.required != state.last_required;
        if (step.progress_changed)
        {
            step.state.last_resident = progress.resident;
            step.state.last_total = progress.total;
            step.state.last_prepared = progress.prepared;
            step.state.last_required = progress.required;
            step.state.last_progress_ms = now_ms;
            step.state.failed = false;
        }

        step.ready = progress.ready() &&
            now_ms - step.state.stable_since_ms >= STABLE_WINDOW_MS;
        step.stalled = !step.ready && !step.state.failed &&
            now_ms - step.state.last_progress_ms >= STALL_TIMEOUT_MS;
        step.state.failed = step.state.failed || step.stalled;
        return step;
    }

    constexpr State retry(State state, uint64_t now_ms)
    {
        state.last_progress_ms = now_ms;
        state.failed = false;
        return state;
    }
}
