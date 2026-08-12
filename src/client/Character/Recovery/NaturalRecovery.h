#pragma once

#include <cstdint>

namespace jrc::natural_recovery
{
    constexpr uint32_t NORMAL_INTERVAL_MS = 10000;
    constexpr int16_t HP_PACKET_LIMIT = 120;
    constexpr int16_t MP_PACKET_LIMIT = 999;

    enum class Posture
    {
        STANDING,
        CLIMBING,
        SITTING,
        OTHER,
        DEAD
    };

    struct State
    {
        uint32_t hp_elapsed_ms = 0;
        uint32_t mp_elapsed_ms = 0;
    };

    struct Context
    {
        Posture posture = Posture::OTHER;
        int32_t hp = 0;
        int32_t max_hp = 0;
        int32_t mp = 0;
        int32_t max_mp = 0;
        uint16_t level = 1;
        int32_t improved_hp = 0;
        int32_t improved_mp = 0;
        int32_t magician_mp_skill_level = 0;
        int32_t endure_interval_seconds = 0;
        int32_t chair_hp = 0;
        int32_t chair_mp = 0;
        float map_recovery = 1.0f;
    };

    struct Tick
    {
        State state;
        int16_t hp_gain = 0;
        int16_t mp_gain = 0;
    };

    int32_t hp_amount(const Context& context);
    int32_t mp_amount(const Context& context);
    Tick advance(State state, const Context& context, uint32_t elapsed_ms);
}
