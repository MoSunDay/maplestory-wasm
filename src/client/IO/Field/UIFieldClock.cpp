#include "UIFieldClock.h"

#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

#include <algorithm>

namespace jrc
{
    namespace
    {
        constexpr int16_t CLOCK_WIDTH = 92;
        constexpr int16_t CLOCK_HEIGHT = 30;
        constexpr int16_t CLOCK_TOP = 16;
    }

    UIFieldClock::UIFieldClock()
        : mode(field_clock::Mode::INACTIVE),
          initial_seconds(0),
          displayed_seconds(-1),
          started_at(std::chrono::steady_clock::now()),
          value_text(Text::A18M, Text::CENTER, Text::WHITE)
    {
        dimension = { CLOCK_WIDTH, CLOCK_HEIGHT };
        active = false;
    }

    void UIFieldClock::draw(float) const
    {
        GraphicsGL::get().drawrectangle(
            position.x(), position.y(), dimension.x(), dimension.y(),
            0.02f, 0.02f, 0.02f, 0.72f
        );
        value_text.draw(position + Point<int16_t>(dimension.x() / 2, 5));
    }

    void UIFieldClock::update()
    {
        if (mode == field_clock::Mode::INACTIVE)
        {
            return;
        }

        const int32_t value = remaining_seconds();
        if (mode == field_clock::Mode::COUNTDOWN && value == 0)
        {
            stop();
            return;
        }

        if (value != displayed_seconds)
        {
            refresh_text();
        }
    }

    void UIFieldClock::update_screen(int16_t new_width, int16_t)
    {
        position = {
            static_cast<int16_t>(std::max<int16_t>(0, (new_width - CLOCK_WIDTH) / 2)),
            CLOCK_TOP
        };
    }

    UIElement::CursorResult UIFieldClock::send_cursor(bool, Point<int16_t>)
    {
        return { Cursor::IDLE, false };
    }

    void UIFieldClock::set_countdown(int32_t seconds)
    {
        if (seconds <= 0)
        {
            stop();
            return;
        }

        mode = field_clock::Mode::COUNTDOWN;
        initial_seconds = seconds;
        displayed_seconds = -1;
        started_at = std::chrono::steady_clock::now();
        active = true;
        refresh_text();
    }

    void UIFieldClock::set_wall_clock(uint8_t hours, uint8_t minutes, uint8_t seconds)
    {
        mode = field_clock::Mode::WALL_CLOCK;
        initial_seconds = static_cast<int32_t>(hours % 24) * 3600
            + static_cast<int32_t>(minutes % 60) * 60
            + static_cast<int32_t>(seconds % 60);
        displayed_seconds = -1;
        started_at = std::chrono::steady_clock::now();
        active = true;
        refresh_text();
    }

    void UIFieldClock::stop()
    {
        mode = field_clock::Mode::INACTIVE;
        initial_seconds = 0;
        displayed_seconds = -1;
        value_text.change_text("");
        active = false;
    }

    int32_t UIFieldClock::remaining_seconds() const
    {
        return field_clock::display_seconds(mode, initial_seconds, elapsed_seconds());
    }

    int64_t UIFieldClock::elapsed_seconds() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - started_at
        ).count();
    }

    void UIFieldClock::refresh_text()
    {
        displayed_seconds = remaining_seconds();
        value_text.change_text(field_clock::format_seconds(
            displayed_seconds,
            mode == field_clock::Mode::WALL_CLOCK
        ));
    }
}
