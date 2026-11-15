// Status bar probes, split from GameplayProbes.cpp to respect the
// per-file line budget.
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIStatusBar.h"

#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"

#include <cstdint>

namespace
{
    constexpr int MISSING_COORDINATE = -32768;
}

extern "C"
{
    const char* msstatusbar_stat_label(int which)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? statusbar->get_stat_label(static_cast<int16_t>(which))
            : "";
    }

    int msstatusbar_hit(int x, int y)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        if (!statusbar)
        {
            return -1;
        }
        const jrc::Point<int16_t> point(
            static_cast<int16_t>(x), static_cast<int16_t>(y));
        return (statusbar->is_active() ? 1 : 0)
            | (statusbar->is_in_range(point) ? 2 : 0);
    }

    int msstatusbar_button_at(int x, int y)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->button_at(jrc::Point<int16_t>(
                static_cast<int16_t>(x), static_cast<int16_t>(y))))
            : -1;
    }

    int msstatusbar_press_count()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_press_count()) : -1;
    }

    int msstatusbar_last_button()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_last_button()) : -1;
    }

    int msstatusbar_cursor_x()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_cursor_x()) : -1;
    }

    int msstatusbar_cursor_y()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_cursor_y()) : -1;
    }

    int msstatusbar_rightclick_index()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_rightclick_index()) : -2;
    }

    int msstatusbar_control_bounds(int id, int edge)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        // Reject ids outside the uint16_t button range so the cast below
        // cannot wrap onto a real button (e.g. 65540 -> BT_MENU).
        if (!statusbar || id < 0 || id > 0xFFFF || edge < 0 || edge > 3)
        {
            return MISSING_COORDINATE;
        }
        const jrc::Rectangle<int16_t> bounds =
            statusbar->get_control_bounds(static_cast<uint16_t>(id));
        if (bounds.width() <= 0 || bounds.height() <= 0)
        {
            return MISSING_COORDINATE;
        }
        switch (edge)
        {
        case 0:
            return static_cast<int>(bounds.l());
        case 1:
            return static_cast<int>(bounds.t());
        case 2:
            return static_cast<int>(bounds.r());
        default:
            return static_cast<int>(bounds.b());
        }
    }
}
