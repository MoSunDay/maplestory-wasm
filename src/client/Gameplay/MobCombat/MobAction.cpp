#include "MobAction.h"

#include <algorithm>
#include <cstdint>

namespace jrc::mob_action
{
    Decision decide(const Context& context)
    {
        if (!context.aggro)
        {
            return Decision::IDLE;
        }
        if (context.skill_ready && context.in_skill_range)
        {
            return Decision::SKILL;
        }
        if (context.attack_ready && context.in_attack_range)
        {
            return Decision::ATTACK;
        }
        if (!context.can_move)
        {
            return Decision::IDLE;
        }
        if (!context.can_fly && context.can_jump && context.on_ground && context.delta_y < -45)
        {
            return Decision::JUMP;
        }
        return Decision::CHASE;
    }

    bool contains(const Range& range, Point<int16_t> origin,
                  Point<int16_t> target, bool facing_right)
    {
        const int32_t direction = facing_right ? 1 : -1;
        const int32_t relative_x = (target.x() - origin.x()) * direction;
        const int32_t relative_y = target.y() - origin.y();

        if (range.shape == Shape::RECTANGLE)
        {
            const int32_t left = std::min<int32_t>(range.lt.x(), range.rb.x());
            const int32_t right = std::max<int32_t>(range.lt.x(), range.rb.x());
            const int32_t top = std::min<int32_t>(range.lt.y(), range.rb.y());
            const int32_t bottom = std::max<int32_t>(range.lt.y(), range.rb.y());
            return relative_x >= left && relative_x <= right
                && relative_y >= top && relative_y <= bottom;
        }
        if (range.shape == Shape::CIRCLE)
        {
            const int64_t dx = relative_x - range.center.x();
            const int64_t dy = relative_y - range.center.y();
            const int64_t radius = range.radius;
            return dx * dx + dy * dy <= radius * radius;
        }
        return false;
    }

    uint8_t normal_activity(uint8_t attack_index, bool facing_right)
    {
        return static_cast<uint8_t>(24 + attack_index * 2 + (facing_right ? 0 : 1));
    }

    uint8_t skill_activity(uint8_t action, bool facing_right)
    {
        return static_cast<uint8_t>(40 + action * 2 + (facing_right ? 0 : 1));
    }
}
