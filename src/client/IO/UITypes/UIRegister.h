#pragma once

#include "../UIElement.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

#include <string>

namespace jrc
{
    class UIRegister : public UIElement
    {
    public:
        static constexpr Type TYPE = REGISTER;
        static constexpr bool FOCUSED = true;
        static constexpr bool TOGGLED = false;

        UIRegister();
        ~UIRegister() override;

        void draw(float alpha) const override;
        void update() override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursor_pos) override;

        void handle_login_failure(int32_t reason);

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum Buttons
        {
            BT_SUBMIT,
            BT_CANCEL
        };

        void submit();
        void cancel();
        void set_pending(bool value);
        void blur_fields();
        void focus_next(Textfield& current, Textfield& next);

        Texture panel_top;
        Texture panel_center;
        Texture field_background;
        Texture panel_bottom;

        Text title;
        Text account_label;
        Text password_label;
        Text confirmation_label;
        Text hint;
        Text status;

        Textfield account;
        Textfield password;
        Textfield confirmation;
        bool pending;
    };
}
