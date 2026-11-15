// Standalone checks for the drag value helpers in src/client/IO/DragSession.cpp.
// No UI element or window dependency: only the free functions are exercised.
#include "IO/DragSession.h"

#include <cstdio>
#include <type_traits>

// The drag session is a move-only value snapshot: Icon owns a unique payload.
static_assert(std::is_move_constructible_v<jrc::DragSession>,
    "DragSession must be movable");
static_assert(!std::is_copy_constructible_v<jrc::DragSession>,
    "DragSession must be move-only (no Icon copies)");
static_assert(!std::is_copy_constructible_v<jrc::Icon>,
    "Icon must not be copyable so drags cannot alias source icons");

namespace
{
    int failures = 0;

    void check(bool condition, const char* name)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", name);
            ++failures;
        }
    }

    using jrc::DRAG_THRESHOLD;
    using jrc::Point;
    using jrc::drag_can_drop_on_stage;
    using jrc::drag_threshold_reached;

    Point<int16_t> at(int x, int y)
    {
        return { static_cast<int16_t>(x), static_cast<int16_t>(y) };
    }
}

int main()
{
    // Threshold itself is 4px (kept honest if it is ever tuned).
    check(DRAG_THRESHOLD == 4, "threshold is 4px");

    // Below the threshold: 3px on either axis is still a click.
    check(!drag_threshold_reached(at(10, 10), at(13, 10)), "3px x is not a drag");
    check(!drag_threshold_reached(at(10, 10), at(10, 13)), "3px y is not a drag");
    check(!drag_threshold_reached(at(10, 10), at(10, 10)), "no movement is not a drag");

    // At the threshold: 4px on either axis starts a drag.
    check(drag_threshold_reached(at(10, 10), at(14, 10)), "4px x is a drag");
    check(drag_threshold_reached(at(10, 10), at(10, 14)), "4px y is a drag");

    // Negative deltas behave symmetrically.
    check(drag_threshold_reached(at(10, 10), at(6, 10)), "-4px x is a drag");
    check(drag_threshold_reached(at(10, 10), at(10, 6)), "-4px y is a drag");
    check(!drag_threshold_reached(at(10, 10), at(7, 10)), "-3px x is not a drag");
    check(!drag_threshold_reached(at(10, 10), at(10, 7)), "-3px y is not a drag");

    // Chebyshev semantics: both axes at 3px stays below the threshold.
    check(!drag_threshold_reached(at(10, 10), at(13, 13)), "diagonal 3,3 is not a drag");
    check(drag_threshold_reached(at(10, 10), at(14, 3)), "axis 4/-7 is a drag");
    check(drag_threshold_reached(at(0, 0), at(0, 4)), "pure y axis at threshold");
    check(drag_threshold_reached(at(0, 0), at(4, 0)), "pure x axis at threshold");
    check(!drag_threshold_reached(at(0, 0), at(0, 3)), "pure y axis below threshold");
    check(!drag_threshold_reached(at(0, 0), at(3, 0)), "pure x axis below threshold");

    // Only items and equipped gear may be dropped on the stage.
    check(drag_can_drop_on_stage(jrc::DragKind::ITEM), "ITEM drops on stage");
    check(drag_can_drop_on_stage(jrc::DragKind::EQUIP), "EQUIP drops on stage");
    check(!drag_can_drop_on_stage(jrc::DragKind::SKILL), "SKILL does not drop on stage");
    check(!drag_can_drop_on_stage(jrc::DragKind::KEYBINDING), "KEYBINDING does not drop on stage");
    check(!drag_can_drop_on_stage(jrc::DragKind::NONE), "NONE does not drop on stage");

    if (failures == 0)
    {
        std::printf("drag-session: all checks passed\n");
        return 0;
    }

    std::printf("drag-session: %d check(s) failed\n", failures);
    return 1;
}
