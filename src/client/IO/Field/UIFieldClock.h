#pragma once

#include "FieldClockModel.h"
#include "../UIElement.h"

#include "../../Graphics/Text.h"

#include <chrono>
#include <cstdint>

namespace jrc
{
    class UIFieldClock : public UIElement
    {
    public:
        static constexpr Type TYPE = FIELDCLOCK;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UIFieldClock();

        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t new_width, int16_t new_height) override;
        CursorResult send_cursor(bool pressed, Point<int16_t> cursorposition) override;

        void set_countdown(int32_t seconds);
        void set_wall_clock(uint8_t hours, uint8_t minutes, uint8_t seconds);
        void stop();
        int32_t remaining_seconds() const;

    private:
        int64_t elapsed_seconds() const;
        void refresh_text();

        field_clock::Mode mode;
        int32_t initial_seconds;
        int32_t displayed_seconds;
        std::chrono::steady_clock::time_point started_at;
        Text value_text;
    };
}
