// Player wall-clip regression: the bottom-of-map rescue in
// FootholdTree::update_fh must re-anchor objects to a real foothold. A
// stranded object with fhid == 0 keeps a dead wall chain (get_wall only
// returns the +-30000 map bounds) and slides through every wall.
#include "Gameplay/Physics/Physics.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    // Mirrors Player::get_walkforce / pet_follow::BASE_MOVEMENT.walk_force.
    constexpr double WALK_FORCE = 0.16;

    struct Fh
    {
        uint16_t id;
        int16_t x1;
        int16_t x2;
        int16_t y1;
        int16_t y2;
        uint16_t prev = 0;
        uint16_t next = 0;
    };

    struct FhSet
    {
        std::vector<Fh> floors;
        std::vector<Fh> walls;
        // Y grows downward and may be negative: seed with the map floor.
        int16_t bottom = -30000;
    };

    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }

    FhSet collect_floors(nl::node fhs)
    {
        FhSet set;
        for (const auto& basef : fhs)
        {
            for (const auto& midf : basef)
            {
                for (const auto& lastf : midf)
                {
                    const auto x1 = static_cast<int16_t>(lastf["x1"].get_integer());
                    const auto x2 = static_cast<int16_t>(lastf["x2"].get_integer());
                    const Fh fh = {
                        static_cast<uint16_t>(std::stoi(lastf.name())), x1, x2,
                        static_cast<int16_t>(lastf["y1"].get_integer()),
                        static_cast<int16_t>(lastf["y2"].get_integer()),
                        static_cast<uint16_t>(lastf["prev"].get_integer()),
                        static_cast<uint16_t>(lastf["next"].get_integer())};
                    if (x1 == x2)
                    {
                        set.walls.push_back(fh);
                    }
                    else
                    {
                        set.floors.push_back(fh);
                    }

                    if (fh.y2 > set.bottom)
                    {
                        set.bottom = fh.y2;
                    }
                }
            }
        }
        return set;
    }

    double ground_at(const Fh& fh, double x)
    {
        const double span = fh.x2 - fh.x1;
        return span ? fh.y1 + (fh.y2 - fh.y1) * (x - fh.x1) / span : fh.y1;
    }

    bool column_covered(const FhSet& set, double x)
    {
        for (const Fh& fh : set.floors)
        {
            if (x >= fh.x1 && x <= fh.x2)
            {
                return true;
            }
        }

        return false;
    }

    const Fh* lowest_floor(const FhSet& set)
    {
        const Fh* lowest = nullptr;
        for (const Fh& fh : set.floors)
        {
            if (!lowest || fh.y2 > lowest->y2)
            {
                lowest = &fh;
            }
        }

        return lowest;
    }

    const Fh* find_fh(const std::vector<Fh>& fhs, uint16_t id)
    {
        for (const Fh& fh : fhs)
        {
            if (fh.id == id)
            {
                return &fh;
            }
        }

        return nullptr;
    }

    // Mirror get_wall's two-hop reach: a wall linked directly or one
    // foothold away blocks movement.
    const Fh* chain_wall(const FhSet& set, const Fh& floor, bool left)
    {
        const uint16_t first = left ? floor.prev : floor.next;
        const Fh* step = find_fh(set.floors, first);
        if (const Fh* wall = find_fh(set.walls, first))
        {
            return wall;
        }

        return step ? find_fh(set.walls, left ? step->prev : step->next)
            : nullptr;
    }

    // The exact state the artificial floor leaves behind: grounded with a
    // dead fhid. It is only acceptable while no foothold covers the column.
    bool ghost_in_covered_column(const jrc::PhysicsObject& object,
        const FhSet& set)
    {
        return object.onground && object.fhid == 0 &&
            column_covered(set, object.crnt_x());
    }

    // Walk off a floor end into an interior void gap, then walk back: the
    // object must re-anchor as soon as its column has a foothold, instead
    // of ghosting along the artificial bottom through the gap's walls.
    bool verify_walkoff_and_return(const jrc::Physics& physics,
        const FhSet& set, const Fh& floor, bool left)
    {
        jrc::PhysicsObject object;
        object.type = jrc::PhysicsObject::NORMAL;
        object.fhid = floor.id;
        object.onground = true;
        const double start_x = left ? floor.x1 + 25.0 : floor.x2 - 25.0;
        object.set_x(start_x);
        object.set_y(ground_at(floor, start_x));

        bool ok = true;
        bool fell = false;
        for (int frame = 0; frame < 600; ++frame)
        {
            object.hforce = fell ? 0.0 : (left ? -WALK_FORCE : WALK_FORCE);
            physics.move_object(object);
            ok &= expect(!ghost_in_covered_column(object, set),
                "walkoff never ghosts in a covered column");
            fell = fell || !object.onground ||
                object.crnt_y() > ground_at(floor, object.crnt_x()) + 4.0;
            if (fell && object.crnt_y() >= set.bottom - 0.5)
            {
                break;
            }
        }
        ok &= expect(fell, "object leaves the floor over the void");

        bool reanchored = false;
        for (int frame = 0; frame < 900 && !reanchored; ++frame)
        {
            object.hforce = left ? WALK_FORCE : -WALK_FORCE;
            physics.move_object(object);
            ok &= expect(!ghost_in_covered_column(object, set),
                "return walk never ghosts in a covered column");
            reanchored = object.onground && object.fhid != 0 &&
                object.crnt_y() < set.bottom - 0.5;
        }
        ok &= expect(reanchored,
            "object re-anchors onto a real foothold after the void fall");
        return ok;
    }

    // A column whose lowest floor links to a wall that rises into the
    // walking window, so the post-rescue wall assertion is checkable.
    const Fh* walled_column_floor(const FhSet& set)
    {
        for (const Fh& fh : set.floors)
        {
            const double mid = (fh.x1 + fh.x2) / 2.0;
            bool lowest_in_column = true;
            for (const Fh& other : set.floors)
            {
                if (other.id != fh.id && other.y2 > fh.y2 &&
                    mid >= other.x1 && mid <= other.x2)
                {
                    lowest_in_column = false;
                    break;
                }
            }

            const double feet = ground_at(fh, mid);
            for (bool left : {true, false})
            {
                const Fh* wall = chain_wall(set, fh, left);
                if (lowest_in_column && wall &&
                    std::min(wall->y1, wall->y2) <= feet - 1.0 &&
                    std::max(wall->y1, wall->y2) >= feet - 50.0)
                {
                    return &fh;
                }
            }
        }

        return nullptr;
    }

    // An object already stranded on the artificial floor must snap onto
    // the lowest real foothold of its column, and that foothold's walls
    // must block it again instead of letting it pass through them.
    bool verify_ghost_rescue(const jrc::Physics& physics, const FhSet& set,
        const Fh& fallback_floor, bool require_wall)
    {
        const Fh* walled = walled_column_floor(set);
        const Fh& target = walled ? *walled : fallback_floor;
        const double x = (target.x1 + target.x2) / 2.0;

        jrc::PhysicsObject object;
        object.type = jrc::PhysicsObject::NORMAL;
        object.fhid = 0;
        object.onground = true;
        object.set_x(x);
        object.set_y(static_cast<double>(set.bottom) + 100.0);

        bool ok = true;
        bool reanchored = false;
        for (int frame = 0; frame < 5 && !reanchored; ++frame)
        {
            object.hforce = WALK_FORCE;
            physics.move_object(object);
            reanchored = object.onground && object.fhid != 0;
        }
        ok &= expect(reanchored,
            "artificial-floor ghost re-anchors to a real foothold");
        ok &= expect(std::abs(object.crnt_y() - ground_at(target, x)) < 2.0,
            "rescued object sits on the lowest foothold surface");
        if (!ok || !walled)
        {
            // Without a blocking wall in this map the re-anchor checks
            // above are all that can be verified here.
            return ok && expect(!require_wall,
                "map has a lowest floor flanked by a blocking wall");
        }

        const double feet = ground_at(target, x);
        const Fh* wall = chain_wall(set, target, true);
        bool toward_left = true;
        if (!wall)
        {
            wall = chain_wall(set, target, false);
            toward_left = false;
        }
        if (!expect(wall != nullptr,
                "rescue floor links to a real wall foothold") ||
            !expect(std::min(wall->y1, wall->y2) <= feet - 1.0 &&
                std::max(wall->y1, wall->y2) >= feet - 50.0,
                "rescued floor's wall rises above the walking window"))
        {
            return false;
        }

        bool saw_wall = false;
        for (int frame = 0; frame < 1200 && !saw_wall; ++frame)
        {
            object.hforce = toward_left ? -WALK_FORCE : WALK_FORCE;
            const jrc::Physics::MoveResult result = physics.move_object(object);
            saw_wall = result.horizontal_collision ==
                jrc::Footholdtree::HorizontalCollision::WALL;
        }
        ok &= expect(saw_wall,
            "rescued object is stopped by the linked wall again");
        return ok;
    }

    bool ran_walkoff = false;

    bool run_map(const nl::node& map_dir, const std::string& name,
        bool require_wall)
    {
        const nl::node fhs = map_dir[name + ".img"]["foothold"];
        if (!fhs)
        {
            std::cerr << "FAIL " << name << " footholds present\n";
            return false;
        }

        const FhSet set = collect_floors(fhs);
        const Fh* floor = lowest_floor(set);
        if (!expect(floor != nullptr, name + " has a floor foothold"))
        {
            return false;
        }

        // Interior void exits only: a floor ending at the map border is
        // guarded by the virtual border wall and can never be walked off.
        int16_t map_l = floor->x1;
        int16_t map_r = floor->x2;
        for (const Fh& fh : set.floors)
        {
            if (fh.x1 < map_l)
            {
                map_l = fh.x1;
            }
            if (fh.x2 > map_r)
            {
                map_r = fh.x2;
            }
        }

        bool ok = true;
        int scenarios = 0;
        for (const Fh& fh : set.floors)
        {
            if (scenarios >= 2)
            {
                break;
            }

            const bool interior_left = fh.x1 > map_l + 60 &&
                !column_covered(set, fh.x1 - 12.0);
            const bool interior_right = fh.x2 < map_r - 60 &&
                !column_covered(set, fh.x2 + 12.0);
            if (interior_left)
            {
                ok &= verify_walkoff_and_return(
                    jrc::Physics(fhs), set, fh, true);
                ++scenarios;
            }
            if (interior_right)
            {
                ok &= verify_walkoff_and_return(
                    jrc::Physics(fhs), set, fh, false);
                ++scenarios;
            }
        }
        ran_walkoff |= scenarios > 0;
        ok &= verify_ghost_rescue(
            jrc::Physics(fhs), set, *floor, require_wall);
        return ok;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "Map.nx";
    nl::file map(path);
    const nl::node map_dir = map.root()["Map"]["Map1"];
    if (!map_dir)
    {
        std::cerr << "FAIL missing Map1 directory\n";
        return 1;
    }

    // 102000001: a single floor spanning the map, void past both borders.
    // 102000000: walled Perion canyons for the artificial-bottom rescue.
    // 100040001: jump-quest tower with interior void gaps to walk off.
    // 100000000: lowest floor flanked by a blocking wall for wall resumption.
    const bool ok = run_map(map_dir, "102000001", false) &&
        run_map(map_dir, "102000000", false) &&
        run_map(map_dir, "100040001", false) &&
        run_map(map_dir, "100000000", true) &&
        expect(ran_walkoff, "found an interior void exit to walk off");
    if (ok)
        std::cout << "PASS player wall-clip foothold rescue physics\n";
    return ok ? 0 : 1;
}
