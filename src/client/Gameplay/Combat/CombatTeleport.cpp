//////////////////////////////////////////////////////////////////////////////
// Teleport target search for the local player's Combat module. Extracted
// from Combat.cpp to keep file sizes within the project limits. Pure
// relocation, no behavior change.
//////////////////////////////////////////////////////////////////////////////
#include "Combat.h"
#include "../../Data/SkillData.h"
#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        // Teleport cooldown in update ticks (~44 * 8ms ≈ 350ms).
        constexpr int16_t TELEPORT_COOLDOWN_TICKS = 44;
        // Default teleport range when skill data has no hrange.
        constexpr int16_t TELEPORT_DEFAULT_RANGE = 130;
        // Step size (px) for horizontal sweep collision checks.
        constexpr int16_t TELEPORT_STEP = 8;
        // Max ground-height delta (px) before treating a step as a wall.
        constexpr int16_t TELEPORT_WALL_THRESHOLD = 35;
    }
    bool Combat::is_teleport_skill(int32_t skillid)
    {
        return skillid == SkillId::TELEPORT_FP
            || skillid == SkillId::IL_TELEPORT
            || skillid == SkillId::PRIEST_TELEPORT;
    }

    bool Combat::apply_teleport(const SpecialMove& move)
    {
        int32_t skillid = move.get_id();
        int32_t level = player.get_skilllevel(skillid);
        const SkillData::Stats& stats = SkillData::get(skillid).get_stats(level);
        int16_t range = static_cast<int16_t>(stats.hrange * 100.0f);
        if (range <= 0)
            range = TELEPORT_DEFAULT_RANGE;

        Point<int16_t> current = player.get_position();
        Point<int16_t> target = find_teleport_target(range);

        if (target == current)
            return false;

        static Animation tp_effect(
            nl::nx::effect["BasicEff.img"]["Teleport"]
        );
        player.show_attack_effect(tp_effect, 0);

        player.set_position(target);
        PhysicsObject& phobj = player.get_phobj();
        phobj.hspeed = 0.0;
        phobj.vspeed = 0.0;
        phobj.fhid = 0;

        teleport_cooldown = TELEPORT_COOLDOWN_TICKS;
        return true;
    }

    Point<int16_t> Combat::find_teleport_target(int16_t range)
    {
        bool up = player.is_key_down(KeyAction::UP);
        bool down = player.is_key_down(KeyAction::DOWN);
        bool left = player.is_key_down(KeyAction::LEFT);
        bool right = player.is_key_down(KeyAction::RIGHT);

        if (left || right)
            return find_teleport_target_horizontal(range);

        if (up || down)
            return find_teleport_target_vertical(up, range);

        return find_teleport_target_horizontal(range);
    }

    Point<int16_t> Combat::find_teleport_target_vertical(bool up, int16_t range)
    {
        Point<int16_t> current = player.get_position();

        if (up)
        {
            // Search from above: get_y_below finds the nearest foothold
            // at or below (y - range), i.e. a platform above us.
            Point<int16_t> above = physics.get_y_below(
                Point<int16_t>(current.x(), current.y() - range)
            );
            // 5px dead-zone avoids teleporting onto the same platform.
            if (above.y() < current.y() - 5)
                return above;
        }
        else
        {
            // +3px skips the current foothold (ground contact is ~y+1)
            // so get_y_below finds the next platform below.
            Point<int16_t> below = physics.get_y_below(
                Point<int16_t>(current.x(), current.y() + 3)
            );
            // 5px dead-zone, and cap at teleport range.
            if (below.y() > current.y() + 5
                && below.y() - current.y() <= range)
                return below;
        }

        return current;
    }

    Point<int16_t> Combat::find_teleport_target_horizontal(int16_t range)
    {
        Point<int16_t> current = player.get_position();

        int16_t direction = player.getflip() ? 1 : -1;
        if (player.is_key_down(KeyAction::LEFT))
            direction = -1;
        else if (player.is_key_down(KeyAction::RIGHT))
            direction = 1;

        Range<int16_t> map_walls = physics.get_fht().get_walls();
        int16_t prev_ground = current.y();
        Point<int16_t> target = current;

        for (int16_t i = 1; i <= range / TELEPORT_STEP; i++)
        {
            int16_t test_x = static_cast<int16_t>(
                std::clamp<int32_t>(
                    current.x() + direction * i * TELEPORT_STEP,
                    map_walls.first(), map_walls.second()
                )
            );

            // -30px searches from above the previous ground level so we
            // snap to the same foothold layer, not one far below.
            int16_t ground_y = physics.get_y_below(
                Point<int16_t>(test_x, prev_ground - 30)
            ).y();

            if (std::abs(static_cast<int32_t>(ground_y)
                       - static_cast<int32_t>(prev_ground)) > TELEPORT_WALL_THRESHOLD)
                break;

            target = Point<int16_t>(test_x, ground_y);
            prev_ground = ground_y;
        }

        return target;
    }

}
