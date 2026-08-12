#include "UIRegister.h"

#include "UILogin.h"
#include "UILoginWait.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Net/Packets/LoginPackets.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace
{
    constexpr size_t ACCOUNT_MIN = 4;
    constexpr size_t ACCOUNT_MAX = 12;
    constexpr size_t PASSWORD_MIN = 4;
    constexpr size_t PASSWORD_MAX = 12;

    bool valid_account(const std::string& value)
    {
        return value.size() >= ACCOUNT_MIN && value.size() <= ACCOUNT_MAX &&
            std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return (c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z');
            });
    }

    bool valid_password(const std::string& value)
    {
        return value.size() >= PASSWORD_MIN && value.size() <= PASSWORD_MAX &&
            std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return c >= 0x21 && c <= 0x7E;
            });
    }
}

namespace jrc
{
    UIRegister::UIRegister()
        : title(Text::A13B, Text::CENTER, Text::DARKGREY, "账号注册"),
          account_label(Text::A11M, Text::LEFT, Text::DARKGREY, "账号"),
          password_label(Text::A11M, Text::LEFT, Text::DARKGREY, "密码"),
          confirmation_label(Text::A11M, Text::LEFT, Text::DARKGREY, "确认密码"),
          hint(Text::A11M, Text::CENTER, Text::DARKGREY,
              "账号4-12位字母或数字，密码4-12位"),
          status(Text::A11M, Text::CENTER, Text::RED, ""),
          account(Text::A13M, Text::LEFT, Text::DARKGREY,
              Rectangle<int16_t>(26, 234, 52, 69), ACCOUNT_MAX),
          password(Text::A13M, Text::LEFT, Text::DARKGREY,
              Rectangle<int16_t>(26, 234, 89, 106), PASSWORD_MAX),
          confirmation(Text::A13M, Text::LEFT, Text::DARKGREY,
              Rectangle<int16_t>(26, 234, 126, 143), PASSWORD_MAX),
          pending(false)
    {
        nl::node basic = nl::nx::ui["Basic.img"];
        nl::node panel = basic["Notice6"];

        panel_top = panel["t"];
        panel_center = panel["c"];
        field_background = panel["box2"];
        panel_bottom = panel["s"];

        buttons[BT_SUBMIT] = std::make_unique<MapleButton>(basic["BtOK4"], 86, 181);
        buttons[BT_CANCEL] = std::make_unique<MapleButton>(basic["BtCancel4"], 136, 181);

        account.set_key_callback(KeyAction::TAB, [&] { focus_next(account, password); });
        password.set_key_callback(KeyAction::TAB, [&] { focus_next(password, confirmation); });
        confirmation.set_key_callback(KeyAction::TAB, [&] { focus_next(confirmation, account); });
        confirmation.set_enter_callback([&](std::string) { submit(); });
        password.set_cryptchar('*');
        confirmation.set_cryptchar('*');
        account.set_state(Textfield::FOCUSED);

        position = { 270, 190 };
        dimension = { 260, 211 };
        active = true;
    }

    UIRegister::~UIRegister()
    {
        blur_fields();
    }

    void UIRegister::draw(float alpha) const
    {
        Point<int16_t> cursor = position;
        panel_top.draw(cursor);
        cursor.shift_y(panel_top.height());
        for (int i = 0; i < 11; ++i)
        {
            panel_center.draw(cursor);
            cursor.shift_y(panel_center.height());
        }
        panel_bottom.draw(cursor);

        title.draw(position + Point<int16_t>(130, 19));
        account_label.draw(position + Point<int16_t>(22, 36));
        password_label.draw(position + Point<int16_t>(22, 73));
        confirmation_label.draw(position + Point<int16_t>(22, 110));
        hint.draw(position + Point<int16_t>(130, 151));
        status.draw(position + Point<int16_t>(130, 166));

        field_background.draw(position + Point<int16_t>(0, 50));
        field_background.draw(position + Point<int16_t>(0, 87));
        field_background.draw(position + Point<int16_t>(0, 124));

        account.draw(position);
        password.draw(position);
        confirmation.draw(position);
        UIElement::draw(alpha);
    }

    void UIRegister::update()
    {
        UIElement::update();
        account.update(position);
        password.update(position);
        confirmation.update(position);
    }

    void UIRegister::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape && !pending)
        {
            cancel();
        }
    }

    UIElement::CursorResult UIRegister::send_cursor(bool clicked, Point<int16_t> cursor_pos)
    {
        if (!pending)
        {
            if (Cursor::State state = account.send_cursor(cursor_pos, clicked))
                return { state, true };
            if (Cursor::State state = password.send_cursor(cursor_pos, clicked))
                return { state, true };
            if (Cursor::State state = confirmation.send_cursor(cursor_pos, clicked))
                return { state, true };
        }
        return UIElement::send_cursor(clicked, cursor_pos);
    }

    Button::State UIRegister::button_pressed(uint16_t id)
    {
        if (pending)
            return Button::DISABLED;

        if (id == BT_SUBMIT)
        {
            submit();
            return Button::NORMAL;
        }
        if (id == BT_CANCEL)
        {
            cancel();
            return Button::NORMAL;
        }
        return Button::PRESSED;
    }

    void UIRegister::submit()
    {
        const std::string& account_value = account.get_text();
        const std::string& password_value = password.get_text();

        if (!valid_account(account_value))
        {
            status.change_text("账号须为4-12位字母或数字");
            return;
        }
        if (!valid_password(password_value))
        {
            status.change_text("密码须为4-12位且不能包含空格");
            return;
        }
        if (password_value != confirmation.get_text())
        {
            status.change_text("两次输入的密码不一致");
            return;
        }

        status.change_text("正在创建账号并登录...");
        set_pending(true);
        UI::get().disable();
        UI::get().emplace<UILoginwait>();
        LoginPacket(account_value, password_value).dispatch();
    }

    void UIRegister::cancel()
    {
        blur_fields();
        active = false;
        if (auto login = UI::get().get_element<UILogin>())
        {
            login->restore_focus();
        }
    }

    void UIRegister::set_pending(bool value)
    {
        pending = value;
        Textfield::State state = value ? Textfield::DISABLED : Textfield::NORMAL;
        account.set_state(state);
        password.set_state(state);
        confirmation.set_state(state);
        buttons[BT_SUBMIT]->set_active(!value);
        buttons[BT_CANCEL]->set_active(!value);
    }

    void UIRegister::handle_login_failure(int32_t reason)
    {
        set_pending(false);
        password.change_text("");
        confirmation.change_text("");
        password.set_state(Textfield::FOCUSED);

        switch (reason)
        {
        case 2:
        case 3:
            status.change_text("该账号或当前连接已被限制");
            break;
        case 4:
            status.change_text("账号已存在或密码不正确");
            break;
        case 5:
            status.change_text("账号注册失败，请稍后重试");
            break;
        case 7:
            status.change_text("该账号当前已登录");
            break;
        case 16:
            status.change_text("请求过于频繁，请稍后重试");
            break;
        default:
            status.change_text("注册失败，错误码 " + std::to_string(reason));
            break;
        }
    }

    void UIRegister::blur_fields()
    {
        if (account.get_state() == Textfield::FOCUSED)
            account.set_state(Textfield::NORMAL);
        if (password.get_state() == Textfield::FOCUSED)
            password.set_state(Textfield::NORMAL);
        if (confirmation.get_state() == Textfield::FOCUSED)
            confirmation.set_state(Textfield::NORMAL);
    }

    void UIRegister::focus_next(Textfield& current, Textfield& next)
    {
        current.set_state(Textfield::NORMAL);
        next.set_state(Textfield::FOCUSED);
    }
}
