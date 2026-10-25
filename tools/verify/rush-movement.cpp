// Pure geometry checks for the warrior rush dash (Hero 1121006,
// Paladin 1221007, Dark Knight 1321003).
#include "Gameplay/Combat/RushMovement.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
    using jrc::Rectangle;
    using namespace jrc::rush_movement;

    // Hero-like range: lt.x = -250, so the hitbox reaches 250px forward.
    constexpr Rectangle<int16_t> HERO_RANGE(-250, 0, -50, 0);
    // Paladin / Dark Knight like symmetric range: lt.x = -500.
    constexpr Rectangle<int16_t> SYMMETRIC_RANGE(-500, 500, -500, 500);

    void verify_hero_reach()
    {
        assert(forward_reach(HERO_RANGE, 1.0f) == 250);
        assert(dash_distance(HERO_RANGE, 1.0f, 400) == 250);
        assert(half_view_distance(800) == 400);
    }

    void verify_symmetric_cap()
    {
        // 800 view -> 400 cap; the 500px reach does not fit.
        assert(half_view_distance(800) == 400);
        assert(dash_distance(
            SYMMETRIC_RANGE, 1.0f, half_view_distance(800)) == 400);

        // 1200 view -> 600 cap; the 500px reach fits, so it is not capped.
        assert(half_view_distance(1200) == 600);
        assert(dash_distance(
            SYMMETRIC_RANGE, 1.0f, half_view_distance(1200)) == 500);
    }

    void verify_symmetric_uses_left_extent()
    {
        // The endpoint comes from lt.x (-500), not from the 1000px width of
        // the symmetric box.
        assert(forward_reach(SYMMETRIC_RANGE, 1.0f) == 500);
        assert(dash_distance(
            SYMMETRIC_RANGE, 1.0f, half_view_distance(1600)) == 500);
        assert(forward_target(0, true, SYMMETRIC_RANGE, 1.0f, 1600) == 500);
    }

    void verify_hrange_scaling()
    {
        assert(forward_reach(HERO_RANGE, 1.5f) == 375);
        assert(dash_distance(HERO_RANGE, 1.5f, 400) == 375);

        // 500px scaled reach is capped by the 400px half-screen limit.
        assert(forward_reach(HERO_RANGE, 2.0f) == 500);
        assert(dash_distance(HERO_RANGE, 2.0f, 400) == 400);

        // Extreme hrange stays inside int32/int16 bounds.
        assert(forward_reach(HERO_RANGE, 200.0f) == 32767);
    }

    void verify_invalid_range()
    {
        // Empty range: no forward extent, falls back to the half-screen cap.
        constexpr Rectangle<int16_t> EMPTY_RANGE;
        assert(forward_reach(EMPTY_RANGE, 1.0f) == 0);
        assert(dash_distance(EMPTY_RANGE, 1.0f, 400) == 400);
        assert(forward_target(1000, true, EMPTY_RANGE, 1.0f, 800) == 1400);

        // A positive lt.x (malformed data) is not a forward reach either.
        constexpr Rectangle<int16_t> MALFORMED_RANGE(250, 0, -50, 0);
        assert(forward_reach(MALFORMED_RANGE, 1.0f) == 0);
        assert(dash_distance(MALFORMED_RANGE, 1.0f, 400) == 400);

        // Zero hrange removes the extent as well.
        assert(forward_reach(HERO_RANGE, 0.0f) == 0);
        assert(dash_distance(HERO_RANGE, 0.0f, 400) == 400);
    }

    void verify_facing()
    {
        assert(target_x(1000, true, 250) == 1250);
        assert(target_x(1000, false, 250) == 750);
        assert(target_x(1000, true, 400) == 1400);
        assert(target_x(1000, false, 400) == 600);

        // Paladin-like endpoint at 800 view width.
        assert(forward_target(1000, true, SYMMETRIC_RANGE, 1.0f, 800) == 1400);
        assert(forward_target(1000, false, SYMMETRIC_RANGE, 1.0f, 800) == 600);

        // Hero-like endpoint, and the same regardless of any mob hit.
        assert(forward_target(1000, true, HERO_RANGE, 1.0f, 800) == 1250);
        assert(forward_target(1000, false, HERO_RANGE, 1.0f, 800) == 750);
    }

    void verify_half_view()
    {
        assert(half_view_distance(800) == 400);
        assert(half_view_distance(1200) == 600);
        assert(half_view_distance(1) == 0);
        assert(half_view_distance(0) == 0);
        assert(half_view_distance(-800) == 0);

        // No cap means no movement, not an unbounded dash.
        assert(dash_distance(HERO_RANGE, 1.0f, 0) == 0);
        assert(dash_distance(HERO_RANGE, 1.0f, -400) == 0);
        assert(forward_target(1000, true, HERO_RANGE, 1.0f, 0) == 1000);
        assert(forward_target(1000, false, HERO_RANGE, 1.0f, 0) == 1000);
    }

    void verify_clamping()
    {
        assert(target_x(32767, true, 400) == 32767);
        assert(target_x(-32768, false, 400) == -32768);
        assert(target_x(32600, true, 250) == 32767);
        assert(target_x(-32600, false, 250) == -32768);
        assert(forward_target(32767, true, HERO_RANGE, 1.0f, 800) == 32767);
        assert(forward_target(-32768, false, HERO_RANGE, 1.0f, 800) == -32768);
    }
}

int main()
{
    verify_hero_reach();
    verify_symmetric_cap();
    verify_symmetric_uses_left_extent();
    verify_hrange_scaling();
    verify_invalid_range();
    verify_facing();
    verify_half_view();
    verify_clamping();

    std::cout << "rush-movement: PASS" << std::endl;
    return 0;
}
