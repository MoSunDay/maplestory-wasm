#pragma once

#include "../../UIElement.h"

#include "../../../Graphics/Geometry.h"
#include "../../../Graphics/Text.h"

namespace jrc
{
    class UIPetOptions : public UIElement
    {
    public:
        static constexpr Type TYPE = PETOPTIONS;
        static constexpr bool FOCUSED = true;
        static constexpr bool TOGGLED = false;

        UIPetOptions();

        void draw(float alpha) const override;
        void update_screen(int16_t width, int16_t height) override;
        CursorResult send_cursor(bool pressed, Point<int16_t> cursorpos) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        void set_threshold(bool hp, int32_t value);
        void sync_labels();
        Rectangle<int16_t> slider_bounds(bool hp) const;

        enum Buttons : uint16_t
        {
            BT_OK,
            BT_CANCEL
        };

        uint8_t hp_threshold;
        uint8_t mp_threshold;
        Texture background2;
        Texture background3;
        ColorBox slider_track;
        ColorBox hp_fill;
        ColorBox mp_fill;
        Text title;
        Text help;
        Text hp_label;
        Text mp_label;
    };
}
