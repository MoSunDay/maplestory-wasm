#pragma once

#include "../../UIElement.h"

#include "../../../Graphics/Geometry.h"
#include "../../../Graphics/Text.h"

#include <cstdint>

namespace jrc
{
    /// In-game display settings window opened from the system menu.
    class UIGameOptions : public UIElement
    {
    public:
        static constexpr Type TYPE = GAMEOPTIONS;
        static constexpr bool FOCUSED = true;
        static constexpr bool TOGGLED = false;

        UIGameOptions();

        void draw(float alpha) const override;
        void update_screen(int16_t width, int16_t height) override;
        CursorResult send_cursor(bool pressed, Point<int16_t> cursorpos) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

        int32_t selected_index() const;
        Point<int16_t> next_center() const;
        Point<int16_t> ok_center() const;

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        enum Buttons : uint16_t
        {
            BT_PREV,
            BT_NEXT,
            BT_OK,
            BT_CANCEL
        };

        void select_resolution(int32_t direction);
        void sync_value_label();
        Point<int16_t> button_center(uint16_t buttonid) const;

        int32_t selected;
        ColorBox panel_background;
        ColorBox panel_header;
        ColorBox section_divider;
        ColorBox value_background;
        ColorBox button_background;
        Text title;
        Text row_label;
        Text value_label;
        Text previous_arrow;
        Text next_arrow;
        Text hint;
        Text ok_label;
        Text cancel_label;
    };
}
