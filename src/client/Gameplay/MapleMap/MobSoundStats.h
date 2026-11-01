// Play counters for mob attack/skill sounds, exposed to e2e drivers
// through Probes/MobSoundProbes.cpp so tests can assert that attacking
// mobs actually trigger sound playback without packet capture.
#pragma once

#include <atomic>
#include <cstdint>

namespace jrc::mobsound
{
    inline std::atomic<int32_t>& played_count()
    {
        static std::atomic<int32_t> value{0};
        return value;
    }

    inline std::atomic<int32_t>& loaded_count()
    {
        static std::atomic<int32_t> value{0};
        return value;
    }

    inline std::atomic<int32_t>& last_played_mob()
    {
        static std::atomic<int32_t> value{0};
        return value;
    }

    inline std::atomic<int32_t>& last_played_action()
    {
        static std::atomic<int32_t> value{0};
        return value;
    }

    inline std::atomic<int32_t>& last_played_skill()
    {
        static std::atomic<int32_t> value{0};
        return value;
    }

    inline void record_loaded()
    {
        loaded_count().fetch_add(1, std::memory_order_relaxed);
    }

    inline void record_played(int32_t mob_id, int32_t action, bool skill)
    {
        played_count().fetch_add(1, std::memory_order_relaxed);
        last_played_mob().store(mob_id, std::memory_order_relaxed);
        last_played_action().store(action, std::memory_order_relaxed);
        last_played_skill().store(skill ? 1 : 0, std::memory_order_relaxed);
    }
}
