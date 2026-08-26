#pragma once

#include "../../UIElement.h"

#include "../../../Graphics/Geometry.h"
#include "../../../Graphics/Text.h"

#include <cstdint>
#include <vector>

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
        int32_t selected_hp_item() const;
        int32_t selected_mp_item() const;

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        void set_threshold(bool hp, int32_t value);
        void select_potion(bool hp, int32_t direction);
        void sync_labels();
        void sync_potion_labels();
        Rectangle<int16_t> slider_bounds(bool hp) const;
        Rectangle<int16_t> potion_bounds(bool hp) const;

        enum Buttons : uint16_t
        {
            BT_OK,
            BT_CANCEL
        };

        uint8_t hp_threshold;
        uint8_t mp_threshold;
        int32_t hp_item;
        int32_t mp_item;
        std::vector<int32_t> hp_candidates;
        std::vector<int32_t> mp_candidates;
        ColorBox panel_background;
        ColorBox panel_header;
        ColorBox section_divider;
        ColorBox slider_track;
        ColorBox hp_fill;
        ColorBox mp_fill;
        ColorBox potion_background;
        ColorBox button_background;
        Text title;
        Text help;
        Text hp_label;
        Text mp_label;
        Text hp_potion_title;
        Text mp_potion_title;
        Text hp_potion_label;
        Text mp_potion_label;
        Text previous_arrow;
        Text next_arrow;
        Text potion_help;
        Text ok_label;
        Text cancel_label;
    };
}
