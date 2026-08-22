#include "PlayerTargeting.h"

#include "../../Template/Rectangle.h"

#include <cstdint>
#include <limits>

namespace jrc
{
    namespace
    {
        constexpr int16_t HIT_HALF_WIDTH = 32;
        constexpr int16_t HIT_TOP = 82;
        constexpr int16_t HIT_BOTTOM = 18;
        constexpr int16_t HIT_CENTER_Y = 32;

        int64_t distance_to_center(Point<int16_t> cursor, Point<int16_t> anchor)
        {
            const int64_t dx = static_cast<int64_t>(cursor.x()) - anchor.x();
            const int64_t dy = static_cast<int64_t>(cursor.y()) -
                (static_cast<int64_t>(anchor.y()) - HIT_CENTER_Y);
            return dx * dx + dy * dy;
        }
    }

    std::optional<PlayerTarget> pick_player_target(
        const std::vector<PlayerTarget>& candidates,
        Point<int16_t> cursor,
        Point<int16_t> view_position)
    {
        std::optional<PlayerTarget> selected;
        int64_t selected_distance = std::numeric_limits<int64_t>::max();

        for (const PlayerTarget& candidate : candidates)
        {
            const Point<int16_t> anchor(
                static_cast<int16_t>(candidate.position.x() + view_position.x()),
                static_cast<int16_t>(candidate.position.y() + view_position.y())
            );
            const Rectangle<int16_t> hitbox(
                static_cast<int16_t>(anchor.x() - HIT_HALF_WIDTH),
                static_cast<int16_t>(anchor.x() + HIT_HALF_WIDTH),
                static_cast<int16_t>(anchor.y() - HIT_TOP),
                static_cast<int16_t>(anchor.y() + HIT_BOTTOM)
            );
            if (!hitbox.contains(cursor))
            {
                continue;
            }

            const int64_t distance = distance_to_center(cursor, anchor);
            if (!selected || distance < selected_distance ||
                (distance == selected_distance && candidate.id < selected->id))
            {
                selected = candidate;
                selected_distance = distance;
            }
        }

        return selected;
    }
}
