#include "UIGameOptions.h"

#include "../../Components/AreaButton.h"
#include "../../Display/Resolution.h"
#include "../../Display/ResolutionStore.h"
#include "../../Display/Viewport.h"
#include "../../UI.h"
#include "../../UILayout.h"
#include "../UIStatusBar.h"

#include "../../../Constants.h"

namespace jrc
{
    namespace
    {
        constexpr int16_t PANEL_WIDTH = 340;
        constexpr int16_t PANEL_HEIGHT = 210;
        constexpr int16_t HEADER_HEIGHT = 40;
        constexpr int16_t DIVIDER_X = 24;
        constexpr int16_t DIVIDER_WIDTH = 292;
        constexpr int16_t HEADER_DIVIDER_Y = 46;
        constexpr int16_t CONTENT_DIVIDER_Y = 152;

        constexpr int16_t ROW_LEFT = 30;
        constexpr int16_t ROW_TOP = 84;
        constexpr int16_t VALUE_LEFT = 86;
        constexpr int16_t VALUE_TOP = 98;
        constexpr int16_t VALUE_WIDTH = 168;
        constexpr int16_t VALUE_HEIGHT = 30;
        constexpr int16_t ARROW_LEFT = 50;
        constexpr int16_t NEXT_ARROW_LEFT = 260;
        constexpr int16_t ARROW_TOP = 98;
        constexpr int16_t ARROW_SIZE = 30;

        constexpr int16_t BUTTON_TOP = 162;
        constexpr int16_t BUTTON_WIDTH = 96;
        constexpr int16_t BUTTON_HEIGHT = 30;
        constexpr int16_t OK_LEFT = 64;
        constexpr int16_t CANCEL_LEFT = 180;

        display::Resolution resolution(int32_t index)
        {
            return display::preset(static_cast<size_t>(index));
        }

        // Accept either a top-left point or a full design box from ui_layout.
        [[maybe_unused]] Point<int16_t> box_origin(Point<int16_t> point)
        {
            return point;
        }

        [[maybe_unused]] Point<int16_t> box_origin(Rectangle<int16_t> box)
        {
            return box.getlt();
        }
    }

    UIGameOptions::UIGameOptions()
        : UIElement({}, { PANEL_WIDTH, PANEL_HEIGHT }),
          selected(static_cast<int32_t>(display::normalize_index(
              static_cast<int>(display::index_of(
                  Constants::viewwidth(), Constants::viewheight()))))),
          panel_background(PANEL_WIDTH, PANEL_HEIGHT, Geometry::BLACK, 0.92f),
          panel_header(PANEL_WIDTH, HEADER_HEIGHT, Geometry::WHITE, 0.12f),
          section_divider(DIVIDER_WIDTH, 1, Geometry::WHITE, 0.28f),
          value_background(VALUE_WIDTH, VALUE_HEIGHT, Geometry::WHITE, 0.10f),
          button_background(BUTTON_WIDTH, BUTTON_HEIGHT, Geometry::WHITE, 0.16f),
          title(Text::A13B, Text::CENTER, Text::WHITE, "GAME OPTIONS"),
          row_label(Text::A12M, Text::LEFT, Text::WHITE, "DISPLAY RESOLUTION"),
          value_label(Text::A12B, Text::CENTER, Text::WHITE, ""),
          previous_arrow(Text::A13B, Text::CENTER, Text::YELLOW, "<"),
          next_arrow(Text::A13B, Text::CENTER, Text::YELLOW, ">"),
          hint(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "Changes apply when you press OK"),
          ok_label(Text::A12B, Text::CENTER, Text::WHITE, "OK"),
          cancel_label(Text::A12B, Text::CENTER, Text::WHITE, "CANCEL")
    {
        buttons[BT_PREV] = std::make_unique<AreaButton>(
            Point<int16_t>(ARROW_LEFT, ARROW_TOP),
            Point<int16_t>(ARROW_SIZE, ARROW_SIZE));
        buttons[BT_NEXT] = std::make_unique<AreaButton>(
            Point<int16_t>(NEXT_ARROW_LEFT, ARROW_TOP),
            Point<int16_t>(ARROW_SIZE, ARROW_SIZE));
        buttons[BT_OK] = std::make_unique<AreaButton>(
            Point<int16_t>(OK_LEFT, BUTTON_TOP),
            Point<int16_t>(BUTTON_WIDTH, BUTTON_HEIGHT));
        buttons[BT_CANCEL] = std::make_unique<AreaButton>(
            Point<int16_t>(CANCEL_LEFT, BUTTON_TOP),
            Point<int16_t>(BUTTON_WIDTH, BUTTON_HEIGHT));
        sync_value_label();
    }

    void UIGameOptions::draw(float alpha) const
    {
        panel_background.draw(position);
        panel_header.draw(position);
        section_divider.draw(position + Point<int16_t>(DIVIDER_X, HEADER_DIVIDER_Y));
        section_divider.draw(position + Point<int16_t>(DIVIDER_X, CONTENT_DIVIDER_Y));
        value_background.draw(position + Point<int16_t>(VALUE_LEFT, VALUE_TOP));
        button_background.draw(position + Point<int16_t>(OK_LEFT, BUTTON_TOP));
        button_background.draw(position + Point<int16_t>(CANCEL_LEFT, BUTTON_TOP));

        title.draw(position + Point<int16_t>(PANEL_WIDTH / 2, 22));
        row_label.draw(position + Point<int16_t>(ROW_LEFT, ROW_TOP));
        value_label.draw(position + Point<int16_t>(PANEL_WIDTH / 2, VALUE_TOP + 8));
        previous_arrow.draw(position + Point<int16_t>(
            ARROW_LEFT + ARROW_SIZE / 2, ARROW_TOP + 7));
        next_arrow.draw(position + Point<int16_t>(
            NEXT_ARROW_LEFT + ARROW_SIZE / 2, ARROW_TOP + 7));
        hint.draw(position + Point<int16_t>(PANEL_WIDTH / 2, 134));
        ok_label.draw(position + Point<int16_t>(OK_LEFT + BUTTON_WIDTH / 2, BUTTON_TOP + 8));
        cancel_label.draw(position + Point<int16_t>(
            CANCEL_LEFT + BUTTON_WIDTH / 2, BUTTON_TOP + 8));

        draw_buttons(alpha);
    }

    void UIGameOptions::update_screen(int16_t, int16_t)
    {
        position = box_origin(
            ui_layout::centered_box(dimension.x(), dimension.y()));
    }

    UIElement::CursorResult UIGameOptions::send_cursor(
        bool pressed, Point<int16_t> cursorpos)
    {
        if (CursorResult button = UIElement::send_cursor(pressed, cursorpos))
            return button;

        return { Cursor::IDLE, false };
    }

    void UIGameOptions::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            deactivate();
    }

    int32_t UIGameOptions::selected_index() const
    {
        return selected;
    }

    Point<int16_t> UIGameOptions::next_center() const
    {
        return button_center(BT_NEXT);
    }

    Point<int16_t> UIGameOptions::ok_center() const
    {
        return button_center(BT_OK);
    }

    Button::State UIGameOptions::button_pressed(uint16_t buttonid)
    {
        switch (buttonid)
        {
        case BT_PREV:
            select_resolution(-1);
            return Button::NORMAL;
        case BT_NEXT:
            select_resolution(1);
            return Button::NORMAL;
        case BT_CANCEL:
            deactivate();
            return Button::NORMAL;
        case BT_OK:
            viewport::apply_runtime(resolution(selected));
            if (!display_store::save_index(static_cast<size_t>(selected)))
            {
                if (Optional<UIStatusbar> statusbar =
                    UI::get().get_element<UIStatusbar>())
                {
                    statusbar->send_chatline(
                        "Unable to save display settings locally",
                        UIChatbar::RED);
                }
            }
            deactivate();
            return Button::NORMAL;
        default:
            return Button::PRESSED;
        }
    }

    void UIGameOptions::select_resolution(int32_t direction)
    {
        selected = static_cast<int32_t>(
            display::next_index(static_cast<size_t>(selected), direction));
        sync_value_label();
    }

    void UIGameOptions::sync_value_label()
    {
        value_label.change_text(display::label(resolution(selected)));
    }

    Point<int16_t> UIGameOptions::button_center(uint16_t buttonid) const
    {
        const auto iter = buttons.find(buttonid);
        if (iter == buttons.end())
            return { -32768, -32768 };

        const Rectangle<int16_t> bounds = iter->second->bounds(position);
        return {
            static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
            static_cast<int16_t>((bounds.t() + bounds.b()) / 2)
        };
    }
}
