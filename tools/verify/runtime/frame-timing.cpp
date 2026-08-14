#include "Character/Recovery/NaturalRecovery.h"
#include "Constants.h"
#include "Runtime/FrameTiming.h"

#include <cassert>
#include <cstdint>

namespace
{
    struct ResumeResult
    {
        int32_t updates = 0;
        int32_t recovery_packets = 0;
        int64_t remainder_us = 0;
    };

    ResumeResult simulate_resume(int64_t elapsed_us)
    {
        using namespace jrc;

        natural_recovery::Context context;
        context.posture = natural_recovery::Posture::STANDING;
        context.hp = 90;
        context.max_hp = 100;
        context.mp = 100;
        context.max_mp = 100;

        natural_recovery::State recovery_state;
        recovery_state.hp_elapsed_ms =
            natural_recovery::NORMAL_INTERVAL_MS - Constants::TIMESTEP;

        constexpr int64_t timestep_us = Constants::TIMESTEP * 1000;
        int64_t accumulator = frame_timing::bounded_elapsed_us(elapsed_us);
        ResumeResult result;

        while (accumulator >= timestep_us)
        {
            auto tick = natural_recovery::advance(
                recovery_state, context, Constants::TIMESTEP);
            recovery_state = tick.state;
            if (tick.hp_gain > 0 || tick.mp_gain > 0)
                result.recovery_packets++;

            result.updates++;
            accumulator -= timestep_us;
        }

        result.remainder_us = accumulator;
        return result;
    }
}

int main()
{
    using namespace jrc::frame_timing;

    assert(bounded_elapsed_us(-1) == 0);
    assert(bounded_elapsed_us(0) == 0);
    assert(bounded_elapsed_us(8000) == 8000);
    assert(bounded_elapsed_us(MAX_CATCH_UP_US) == MAX_CATCH_UP_US);
    assert(bounded_elapsed_us(160000000) == MAX_CATCH_UP_US);

    ResumeResult normal = simulate_resume(16000);
    assert(normal.updates == 2);
    assert(normal.recovery_packets == 1);
    assert(normal.remainder_us == 0);

    ResumeResult resumed = simulate_resume(160000000);
    assert(resumed.updates == 31);
    assert(resumed.recovery_packets == 1);
    assert(resumed.remainder_us == 2000);
}
