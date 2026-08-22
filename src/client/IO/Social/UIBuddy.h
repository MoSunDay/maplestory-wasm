#pragma once

#include "SocialState.h"
#include "../UIDragElement.h"
#include "../Components/Textfield.h"
#include "../../Graphics/Geometry.h"

#include <array>
#include <map>

namespace jrc
{
    class UIBuddy : public UIDragElement<PosBUDDY>
    {
    public:
        static constexpr Type TYPE = UIElement::BUDDY;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        UIBuddy();

        void draw(float alpha) const override;
        void update() override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursorpos) override;
        void send_scroll(double yoffset) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        void set_state(const social::BuddyState& state);
        Point<int16_t> origin() const { return position; }

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        enum Buttons : uint16_t
        {
            BT_CLOSE,
            BT_ADD,
            BT_DELETE,
            BT_ACCEPT,
            BT_DECLINE
        };

        static constexpr size_t VISIBLE_ROWS = 6;
        static constexpr int16_t ROW_TOP = 46;
        static constexpr int16_t ROW_HEIGHT = 18;

        void add_buddy();
        void sync_text();
        void sync_buttons();
        void draw_button_labels() const;

        ColorBox background;
        ColorBox header;
        ColorBox divider;
        Text title;
        Text summary;
        Text pending;
        Text add_label;
        std::array<Text, VISIBLE_ROWS> rows;
        std::map<uint16_t, Text> button_labels;
        Textfield add_field;
        social::BuddyState state;
        int32_t selected_id = -1;
        size_t first_row = 0;
    };
}
