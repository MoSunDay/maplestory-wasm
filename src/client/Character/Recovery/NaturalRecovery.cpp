#include "NaturalRecovery.h"

#include <algorithm>
#include <cmath>

namespace jrc::natural_recovery
{
    namespace
    {
        int32_t apply_map_rate(int32_t amount, float rate)
        {
            return static_cast<int32_t>(std::floor(amount * std::max(rate, 0.0f)));
        }

        bool hp_eligible(const Context& context)
        {
            if (context.posture == Posture::STANDING || context.posture == Posture::SITTING)
                return true;

            return context.posture == Posture::CLIMBING
                && context.endure_interval_seconds > 0;
        }

        uint32_t hp_interval(const Context& context)
        {
            if (context.posture == Posture::CLIMBING)
                return static_cast<uint32_t>(context.endure_interval_seconds) * 1000;

            return NORMAL_INTERVAL_MS;
        }
    }

    int32_t hp_amount(const Context& context)
    {
        int32_t natural = 10 + std::max(context.improved_hp, 0);
        int32_t amount = std::max(natural, std::max(context.chair_hp, 0));
        return std::clamp(apply_map_rate(amount, context.map_recovery), 0,
            static_cast<int32_t>(HP_PACKET_LIMIT));
    }

    int32_t mp_amount(const Context& context)
    {
        int32_t natural = 3 + std::max(context.improved_mp, 0);
        if (context.magician_mp_skill_level > 0)
        {
            natural = 3 + context.magician_mp_skill_level * context.level / 10;
        }

        int32_t amount = std::max(natural, std::max(context.chair_mp, 0));
        return std::clamp(apply_map_rate(amount, context.map_recovery), 0,
            static_cast<int32_t>(MP_PACKET_LIMIT));
    }

    Tick advance(State state, const Context& context, uint32_t elapsed_ms)
    {
        Tick tick{ state };
        if (context.posture == Posture::DEAD)
        {
            tick.state = {};
            return tick;
        }

        if (hp_eligible(context))
            tick.state.hp_elapsed_ms += elapsed_ms;
        else
            tick.state.hp_elapsed_ms = 0;

        tick.state.mp_elapsed_ms += elapsed_ms;

        uint32_t interval = hp_interval(context);
        if (hp_eligible(context) && tick.state.hp_elapsed_ms >= interval)
        {
            tick.state.hp_elapsed_ms %= interval;
            if (context.hp < context.max_hp)
                tick.hp_gain = static_cast<int16_t>(hp_amount(context));
        }

        if (tick.state.mp_elapsed_ms >= NORMAL_INTERVAL_MS)
        {
            tick.state.mp_elapsed_ms %= NORMAL_INTERVAL_MS;
            if (context.mp < context.max_mp)
                tick.mp_gain = static_cast<int16_t>(mp_amount(context));
        }

        return tick;
    }
}
