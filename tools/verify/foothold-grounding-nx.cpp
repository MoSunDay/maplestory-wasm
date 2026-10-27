#include "../../src/client/Gameplay/Physics/Physics.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    // Map 102000002 (Perion): flat upper platform fh 29 (x -251..-159,
    // y 117) above the lower slope chain (fh 15..18, y ~167..179), plus
    // slope fh 27 from (254, 175) to (286, 144).
    constexpr uint16_t UPPER_FH = 29;
    constexpr double UPPER_X = -235.0;
    constexpr double UPPER_Y = 117.0;
    constexpr double SLOPE_FH = 27.0;
    constexpr double SLOPE_X = 270.0;
    // Interpolated slope surface at x=270: non-integer by construction, so
    // integer spawn coordinates can never equal it exactly.
    constexpr double SLOPE_Y = 159.5;

    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }

    // A fresh spawn/teleport: no foothold knowledge, optionally moving.
    jrc::PhysicsObject spawned_object(double x, double y, double vspeed)
    {
        jrc::PhysicsObject object;
        object.set_x(x);
        object.set_y(y);
        object.fhid = 0;
        object.onground = false;
        object.vspeed = vspeed;
        object.type = jrc::PhysicsObject::NORMAL;
        return object;
    }

    bool above_map_bottom(const jrc::Physics& physics, double y)
    {
        return y < physics.get_fht().get_borders().second() - 100.0;
    }

    // Integer spawn cy 0.8px below a flat surface (OrbisPQ-style portal
    // point): the platform beneath must catch the object.
    bool verify_spawn_below_flat(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(
            UPPER_X, UPPER_Y + 0.8, 0.0);
        bool ok = true;
        for (int frame = 0; frame < 60; ++frame)
        {
            const jrc::Physics::MoveResult result = physics.move_object(
                object);
            ok &= expect(result.grounded,
                "spawn 0.8px below a flat surface grounds immediately");
            ok &= expect(object.crnt_y() == UPPER_Y,
                "spawn below a flat surface rests on the surface");
        }
        ok &= expect(object.fhid == UPPER_FH,
            "spawn below a flat surface acquires the platform fhid");
        ok &= expect(above_map_bottom(physics, object.crnt_y()),
            "spawn below a flat surface never reaches the map bottom");
        return ok;
    }

    // Integer spawn cy 0.8px below an interpolated slope surface: the slope
    // must catch the object instead of blacklisting itself.
    bool verify_spawn_below_slope(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(
            SLOPE_X, SLOPE_Y + 0.8, 0.0);
        bool ok = true;
        for (int frame = 0; frame < 60; ++frame)
        {
            const jrc::Physics::MoveResult result = physics.move_object(
                object);
            ok &= expect(result.grounded,
                "spawn 0.8px below a slope surface grounds immediately");
        }
        ok &= expect(std::abs(object.crnt_y() - SLOPE_Y) <= 5.0,
            "spawn below a slope rests on the slope surface");
        ok &= expect(static_cast<double>(object.fhid) == SLOPE_FH,
            "spawn below a slope acquires the slope fhid");
        ok &= expect(above_map_bottom(physics, object.crnt_y()),
            "spawn below a slope never reaches the map bottom");
        return ok;
    }

    // Regression: spawning above a surface still lands by crossing it.
    bool verify_spawn_above(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(
            UPPER_X, UPPER_Y - 1.0, 0.0);
        bool ok = true;
        bool landed = false;
        for (int frame = 0; frame < 120 && !landed; ++frame)
            landed = physics.move_object(object).grounded;
        ok &= expect(landed, "spawn 1px above the surface lands");
        ok &= expect(object.crnt_y() == UPPER_Y,
            "landing from above rests exactly on the surface");
        ok &= expect(object.fhid == UPPER_FH,
            "landing from above acquires the platform fhid");
        return ok;
    }

    // Rising objects pass through the surface from below: the tolerance
    // band must only absorb objects that are not moving up. The initial
    // speed keeps the arc apex (~y 85) below the platform overhead at
    // x=-235 (fh 30, y ~70.7) so the object falls back onto fh 29.
    bool verify_rising_passes_through(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(
            UPPER_X, UPPER_Y + 0.8, -3.0);
        bool ok = true;
        const jrc::Physics::MoveResult first = physics.move_object(object);
        ok &= expect(!first.grounded,
            "rising object below the surface is not grounded");
        ok &= expect(object.crnt_y() < UPPER_Y,
            "rising object passes through the surface");
        bool landed = false;
        for (int frame = 0; frame < 300 && !landed; ++frame)
            landed = physics.move_object(object).grounded;
        ok &= expect(landed,
            "after the jump arc the object lands on the platform");
        ok &= expect(object.crnt_y() == UPPER_Y,
            "jump arc lands exactly on the surface");
        return ok;
    }

    // Alt+Down jump-down (PlayerStates::try_jump_down sets y := groundbelow):
    // the drop start lies below the tolerance band and must pass through.
    bool verify_jump_down(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(UPPER_X, UPPER_Y, 0.0);
        object.fhid = UPPER_FH;
        object.onground = true;
        object.set_flag(jrc::PhysicsObject::CHECKBELOW);

        bool ok = true;
        const jrc::Physics::MoveResult armed = physics.move_object(object);
        ok &= expect(armed.grounded, "standing object stays grounded");
        ok &= expect(object.enablejd,
            "jump-down is armed while a floor exists below");
        ok &= expect(object.groundbelow > UPPER_Y + 5.0,
            "jump-down start lies below the tolerance band");

        object.y = object.groundbelow;
        bool dropped = false;
        for (int frame = 0; frame < 240; ++frame)
        {
            const jrc::Physics::MoveResult result = physics.move_object(
                object);
            dropped = dropped || !result.grounded;
        }
        ok &= expect(dropped, "jump-down leaves the upper platform");
        ok &= expect(object.onground, "jump-down lands on the lower floor");
        ok &= expect(object.fhid != UPPER_FH,
            "jump-down ends on a different foothold");
        ok &= expect(object.crnt_y() > UPPER_Y + 10.0,
            "jump-down ends below the upper platform");
        ok &= expect(object.crnt_y() < UPPER_Y + 80.0,
            "jump-down lands on the lower chain, not the map bottom");
        return ok;
    }

    // OrbisPQ 200080101 regression (the map that motivated the wz cy
    // patch): NPC life cy=-41 sits 0.8px below the interpolated surface of
    // slope fh 14 ((138,-36)->(231,-45), surface -41.8 at x=198). With zero
    // tolerance the slope was blacklisted and the object locked onto the
    // lower floor fh 26 (~y 168), 215px below; the band must catch it.
    bool verify_orbis_pq_slope(const jrc::Physics& physics)
    {
        constexpr double NPC_X = 198.0;
        constexpr double DEFECT_CY = -41.0;
        jrc::PhysicsObject object = spawned_object(NPC_X, DEFECT_CY, 0.0);
        bool ok = true;
        for (int frame = 0; frame < 60; ++frame)
        {
            const jrc::Physics::MoveResult result = physics.move_object(
                object);
            ok &= expect(result.grounded,
                "OrbisPQ cy=-41 grounds on the slope above");
            ok &= expect(std::abs(object.crnt_y() + 41.8) <= 5.0,
                "OrbisPQ spawn rests on the fh14 slope surface (~-41.8)");
        }
        ok &= expect(object.fhid == 14,
            "OrbisPQ spawn acquires slope fh14");
        ok &= expect(object.crnt_y() < 0.0,
            "OrbisPQ spawn stays on the upper layer, not fh26 (~168)");
        return ok;
    }

    // Guard against over-snapping: 20px below the upper platform is outside
    // the tolerance band, so the object must fall to the lower chain.
    bool verify_no_catch_when_far_below(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = spawned_object(
            UPPER_X, UPPER_Y + 20.0, 0.0);
        bool ok = true;
        bool grounded = false;
        for (int frame = 0; frame < 240 && !grounded; ++frame)
            grounded = physics.move_object(object).grounded;
        ok &= expect(grounded, "object far below lands on the lower chain");
        ok &= expect(object.crnt_y() > UPPER_Y + 10.0,
            "object far below is not pulled up to the upper platform");
        ok &= expect(object.fhid != UPPER_FH,
            "object far below does not acquire the upper platform");
        return ok;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "Map.nx";
    nl::file map(path);
    const nl::node footholds =
        map.root()["Map"]["Map1"]["102000002.img"]["foothold"];
    if (!footholds)
    {
        std::cerr << "FAIL missing Perion footholds\n";
        return 1;
    }

    nl::node orbisfhs =
        map.root()["Map"]["Map2"]["200080101.img"]["foothold"];
    if (!orbisfhs)
    {
        std::cerr << "FAIL missing OrbisPQ footholds\n";
        return 1;
    }

    const jrc::Physics physics(footholds);
    const jrc::Physics orbis(orbisfhs);
    const bool ok = verify_spawn_below_flat(physics) &&
        verify_spawn_below_slope(physics) && verify_spawn_above(physics) &&
        verify_rising_passes_through(physics) && verify_jump_down(physics) &&
        verify_no_catch_when_far_below(physics) &&
        verify_orbis_pq_slope(orbis);
    if (ok)
        std::cout << "PASS foothold grounding tolerance band: below-surface"
            " spawns land, above-surface crossings land, rising objects"
            " pass through, jump-down drops, far-below objects fall"
            " to the lower chain, OrbisPQ cy=-41 grounds on its slope\n";
    return ok ? 0 : 1;
}
