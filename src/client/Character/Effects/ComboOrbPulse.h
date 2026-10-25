#pragma once

#include <cmath>
#include <cstdint>

namespace jrc::combo_pulse
{
    constexpr float FAR_SHADE = 0.65f;
    constexpr float TAU = 6.28318531f;

    // Loop-continuous glow envelope in [0, 1]. The orb 'state' frames each
    // carry an a0 -> a1 alpha ramp, and playing those ramps back to back
    // snaps the opacity from a1 back down to a0 at every frame boundary (a
    // hard blink with the shipped a0=64/a1=255/delay=300 data). Breathing
    // once per full frame loop instead glides between the data's dimmest
    // start and brightest end without any snap.
    inline float breathe(int64_t t, int64_t duration)
    {
        if (duration <= 0)
            return 0.0f;
        const int64_t wrapped = ((t % duration) + duration) % duration;
        const float phase = static_cast<float>(wrapped)
            / static_cast<float>(duration);
        return 0.5f - 0.5f * std::cos(TAU * phase);
    }

    // Continuous near/far shading for an orb at relative orbit height
    // 'height' (= sin(angle), in [-1, 1]): FAR_SHADE at the far end of the
    // orbit, 1.0 at the near end, so the dimming follows the orbit smoothly
    // instead of popping when the orb crosses the horizontal diameter.
    inline float depth_shade(float height)
    {
        return FAR_SHADE + (1.0f - FAR_SHADE) * (0.5f + 0.5f * height);
    }
}
