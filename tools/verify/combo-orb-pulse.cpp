#include <cassert>
#include <cmath>

#include "Character/Effects/ComboOrbPulse.h"

namespace
{
    bool near(float actual, float expected)
    {
        return std::fabs(actual - expected) < 1e-4f;
    }
}

int main()
{
    using jrc::combo_pulse::breathe;
    using jrc::combo_pulse::depth_shade;

    // The shipped loop data: five state frames of delay 300 with a0=64,
    // a1=255, so the frame loop lasts 1500 ms.
    constexpr int64_t DURATION = 1500;

    // Endpoints of the envelope and loop continuity. cos(TAU) == cos(0) with
    // the same wrapped phase, so the wrap points compare exactly equal.
    assert(near(breathe(0, DURATION), 0.0f));
    assert(near(breathe(750, DURATION), 1.0f));
    assert(breathe(1500, DURATION) == breathe(0, DURATION));
    assert(breathe(3000, DURATION) == breathe(0, DURATION));
    assert(breathe(-DURATION, DURATION) == breathe(0, DURATION));

    // Degenerate durations are steady, not crashes.
    assert(breathe(123, 0) == 0.0f);
    assert(breathe(123, -5) == 0.0f);

    // Monotonic rise on [0, 750] and fall on [750, 1500], 25 ms steps.
    for (int64_t t = 0; t < 750; t += 25)
        assert(breathe(t, DURATION) <= breathe(t + 25, DURATION));
    for (int64_t t = 750; t < 1500; t += 25)
        assert(breathe(t, DURATION) >= breathe(t + 25, DURATION));

    // The envelope stays inside [0, 1].
    for (int64_t t = 0; t <= DURATION; t += 25)
    {
        assert(breathe(t, DURATION) >= 0.0f);
        assert(breathe(t, DURATION) <= 1.0f);
    }

    // No-flicker bound on the rendered opacity, the quantity that used to
    // snap: the old per-frame ramp jumped from a1=255 straight back to
    // a0=64, an instant 191/255 ~= 0.75 step every 300 ms. Sampling the
    // breathing pulse (64..255 over the 1500 ms loop) every 25 ms, the worst
    // step is 191/255 * sin(PI * 25 / 1500) ~= 0.039.
    constexpr int16_t GLOW_MIN = 64;
    constexpr int16_t GLOW_MAX = 255;
    float worst_step = 0.0f;
    float previous = static_cast<float>(GLOW_MIN) / 255.0f;
    for (int64_t t = 25; t <= DURATION; t += 25)
    {
        const float pulse = static_cast<float>(GLOW_MIN)
            + static_cast<float>(GLOW_MAX - GLOW_MIN)
                * breathe(t, DURATION);
        const float opacity = pulse / 255.0f;
        const float step = std::fabs(opacity - previous);
        if (step > worst_step)
            worst_step = step;
        previous = opacity;
    }
    assert(worst_step < 0.05f);

    // Depth shading spans far..near without a jump on the axis crossing.
    assert(near(depth_shade(-1.0f), 0.65f));
    assert(near(depth_shade(1.0f), 1.0f));
    assert(near(depth_shade(0.0f), 0.825f));
    assert(std::fabs(depth_shade(1e-4f) - depth_shade(-1e-4f)) < 1e-4f);
    for (float height = -1.0f; height < 1.0f; height += 0.1f)
        assert(depth_shade(height) <= depth_shade(height + 0.1f));
}
