#include "../../src/client/Gameplay/Pet/PetFollowPolicy.h"
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
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }

    double ground(double x1, double y1, double x2, double y2, double x)
    {
        return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
    }

    jrc::PhysicsObject grounded_object(double x, double y, uint16_t foothold)
    {
        jrc::PhysicsObject object;
        object.set_x(x);
        object.set_y(y);
        object.fhid = foothold;
        object.onground = true;
        object.type = jrc::PhysicsObject::NORMAL;
        return object;
    }

    bool verify_slope(const jrc::Physics& physics, bool right)
    {
        constexpr double X1 = 254.0;
        constexpr double Y1 = 175.0;
        constexpr double X2 = 286.0;
        constexpr double Y2 = 144.0;
        const double start_x = right ? 258.0 : 282.0;
        const double target_x = right ? 280.0 : 260.0;
        jrc::PhysicsObject object = grounded_object(
            start_x, ground(X1, Y1, X2, Y2, start_x), 27);
        bool ok = true;
        bool reached = false;

        for (int frame = 0; frame < 240; ++frame)
        {
            const auto movement = right ? jrc::pet_follow::Action::MOVE_RIGHT :
                jrc::pet_follow::Action::MOVE_LEFT;
            object.hforce = right ? jrc::pet_follow::BASE_MOVEMENT.walk_force :
                -jrc::pet_follow::BASE_MOVEMENT.walk_force;
            const jrc::Physics::MoveResult result = physics.move_object(object);
            const bool wall = result.horizontal_collision ==
                jrc::Footholdtree::HorizontalCollision::WALL;
            ok &= expect(result.grounded, "connected slope remains grounded");
            ok &= expect(!wall, "connected slope is not a wall collision");
            ok &= expect(!jrc::pet_follow::should_jump({ movement,
                result.grounded, wall, result.start_x, result.end_x }),
                "connected slope never requests a jump");

            if (object.fhid == 27)
            {
                const double expected_y = ground(
                    X1, Y1, X2, Y2, object.crnt_x());
                ok &= expect(std::abs(object.crnt_y() - expected_y) < 2.0,
                    "pet remains attached to slope height");
            }
            reached = right ? object.crnt_x() >= target_x :
                object.crnt_x() <= target_x;
            if (reached)
                break;
        }

        return expect(reached, right ? "pet advances uphill" :
            "pet advances downhill") && ok;
    }

    bool verify_wall(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = grounded_object(10.0, 121.0, 1);
        bool saw_wall = false;
        bool saw_jump_request = false;
        for (int frame = 0; frame < 180; ++frame)
        {
            object.hforce = jrc::pet_follow::BASE_MOVEMENT.walk_force;
            const jrc::Physics::MoveResult result = physics.move_object(object);
            const bool wall = result.horizontal_collision ==
                jrc::Footholdtree::HorizontalCollision::WALL;
            saw_wall = saw_wall || wall;
            saw_jump_request = saw_jump_request ||
                jrc::pet_follow::should_jump({
                    jrc::pet_follow::Action::MOVE_RIGHT, result.grounded,
                    wall, result.start_x, result.end_x });
        }
        return expect(saw_wall, "real wall reports horizontal blockage") &&
            expect(saw_jump_request,
                "fully blocked movement against a wall requests a jump");
    }

    // Two stacked platforms: flat fh 29 (x -251..-159, y 117) with the lower
    // slope chain (fh 15..18, y ~167..179) underneath at overlapping x.
    bool verify_warp_anchor(const jrc::Physics& physics)
    {
        constexpr uint16_t UPPER_FH = 29;
        constexpr uint16_t LOWER_FH = 18;
        constexpr double WARP_X = -235.0;
        constexpr double OWNER_Y = 117.0;
        bool ok = true;

        // PetLook::warp_to with the owner's fhid: the leash lands the pet on
        // the owner's platform and it must stay there.
        jrc::PhysicsObject anchored = grounded_object(
            -200.0, 176.0, LOWER_FH);
        anchored.set_x(WARP_X);
        anchored.set_y(OWNER_Y);
        jrc::pet_follow::anchor_after_warp(anchored, UPPER_FH);
        for (int frame = 0; frame < 60; ++frame)
        {
            const jrc::Physics::MoveResult result = physics.move_object(
                anchored);
            ok &= expect(result.grounded,
                "anchored warp stays grounded on the owner's platform");
            ok &= expect(anchored.crnt_y() == OWNER_Y,
                "anchored warp never sinks below the owner");
        }

        // Pre-fix warp_to kept the stale lower foothold: one frame of gravity
        // puts the pet below the owner's surface, get_fhid_below skips the
        // overhead platform and locks onto the floor below.
        jrc::PhysicsObject stale = grounded_object(-200.0, 176.0, LOWER_FH);
        stale.set_x(WARP_X);
        stale.set_y(OWNER_Y);
        for (int frame = 0; frame < 60; ++frame)
            physics.move_object(stale);
        ok &= expect(!stale.onground || stale.crnt_y() - OWNER_Y >
            jrc::pet_follow::HEIGHT_TOLERANCE,
            "stale foothold after warp drops off the owner's platform");
        return ok;
    }

    // Follow mode re-arms TURNATEDGES every frame: the pet must stop at a
    // terminal edge. Only a loot dive (flag cleared) may leave the platform.
    bool verify_edge_stop(const jrc::Physics& physics)
    {
        jrc::PhysicsObject object = grounded_object(-200.0, 117.0, 29);
        bool ok = true;
        bool reached_edge = false;
        for (int frame = 0; frame < 240; ++frame)
        {
            object.hforce = jrc::pet_follow::BASE_MOVEMENT.walk_force;
            object.set_flag(jrc::PhysicsObject::TURNATEDGES);
            const jrc::Physics::MoveResult result = physics.move_object(
                object);
            ok &= expect(result.grounded,
                "edge-disciplined pet stays grounded");
            ok &= expect(object.crnt_y() == 117.0,
                "edge-disciplined pet keeps the platform height");
            ok &= expect(result.horizontal_collision !=
                jrc::Footholdtree::HorizontalCollision::WALL,
                "terminal edge is not reported as a wall");
            ok &= expect(object.crnt_x() <= -158.5,
                "edge-disciplined pet stops at the terminal edge");
            reached_edge = reached_edge || object.crnt_x() >= -159.5;
        }
        ok &= expect(reached_edge, "edge-disciplined pet still reaches the edge");

        jrc::PhysicsObject diver = grounded_object(-200.0, 117.0, 29);
        bool dropped = false;
        for (int frame = 0; frame < 240 && !dropped; ++frame)
        {
            diver.hforce = jrc::pet_follow::BASE_MOVEMENT.walk_force;
            dropped = !physics.move_object(diver).grounded;
        }
        ok &= expect(dropped,
            "loot dive without the edge flag still leaves the platform");
        return ok;
    }

    bool verify_edge(const jrc::Physics& physics)
    {
        constexpr double X1 = -263.0;
        constexpr double Y1 = 71.0;
        constexpr double X2 = -172.0;
        constexpr double Y2 = 70.0;
        jrc::PhysicsObject object = grounded_object(
            -180.0, ground(X1, Y1, X2, Y2, -180.0), 30);
        bool airborne = false;
        bool ok = true;
        for (int frame = 0; frame < 180; ++frame)
        {
            object.hforce = jrc::pet_follow::BASE_MOVEMENT.walk_force;
            const jrc::Physics::MoveResult result = physics.move_object(object);
            const bool wall = result.horizontal_collision ==
                jrc::Footholdtree::HorizontalCollision::WALL;
            ok &= expect(!wall, "terminal edge is not a wall collision");
            ok &= expect(!jrc::pet_follow::should_jump({
                jrc::pet_follow::Action::MOVE_RIGHT, result.grounded,
                wall, result.start_x, result.end_x }),
                "walking off a terminal edge never requests a jump");
            airborne = airborne || !result.grounded;
            if (airborne)
                break;
        }
        return expect(airborne, "ordinary movement walks off the platform") && ok;
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

    const jrc::Physics physics(footholds);
    const bool ok = verify_slope(physics, true) &&
        verify_slope(physics, false) && verify_wall(physics) &&
        verify_edge(physics) && verify_warp_anchor(physics) &&
        verify_edge_stop(physics);
    if (ok)
        std::cout << "PASS pet slope, wall, terminal-edge, warp-anchor, "
            "and edge-discipline physics\n";
    return ok ? 0 : 1;
}
