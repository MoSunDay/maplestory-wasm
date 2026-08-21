#include "ImeBridge.h"

#include "Components/Textfield.h"
#include "CashShop/UICashShop.h"
#include "UI.h"
#include "UITypes/UICharCreation.h"
#include "UITypes/UILoginNotice.h"
#include "UITypes/UIWorldSelect.h"

#ifdef MS_PLATFORM_WASM

#include <emscripten.h>

#include <GLFW/glfw3.h>

namespace jrc
{
    namespace
    {
        bool active = false;
        const Textfield* focusedfield = nullptr;

        // The JS side forwards DOM key codes; convert the small whitelist to
        // GLFW codes so UI::send_key sees its usual key space.
        int32_t dom_to_glfw(int32_t dom_keycode)
        {
            switch (dom_keycode)
            {
            case 13: return GLFW_KEY_ENTER;
            case 9:  return GLFW_KEY_TAB;
            case 27: return GLFW_KEY_ESCAPE;
            case 38: return GLFW_KEY_UP;
            case 40: return GLFW_KEY_DOWN;
            default: return 0;
            }
        }
    }

    bool ImeBridge::is_active()
    {
        return active;
    }

    void ImeBridge::focus_field(Textfield* field)
    {
        if (field->is_crypted())
        {
            // Password fields stay on the plain keyboard path.
            return;
        }

        active = true;
        focusedfield = field;

        Rectangle<int16_t> bounds = field->get_bounds();
        std::string text = field->get_text();
        int caret = static_cast<int>(field->caret_utf16());

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onFocus)
            {
                window.MapleWasmIME.onFocus($0, $1, $2, $3, UTF8ToString($4), $5);
            }
        },
            bounds.getlt().x(), bounds.getlt().y(), bounds.width(), bounds.height(),
            text.c_str(), caret);
    }

    void ImeBridge::blur_field()
    {
        if (!active)
        {
            return;
        }

        active = false;
        focusedfield = nullptr;

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onBlur)
            {
                window.MapleWasmIME.onBlur();
            }
        });
    }

    void ImeBridge::sync_field(const Textfield* field)
    {
        if (!active || field != focusedfield)
        {
            return;
        }

        std::string text = field->get_text();
        int caret = static_cast<int>(field->caret_utf16());

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onText)
            {
                window.MapleWasmIME.onText(UTF8ToString($0), $1);
            }
        }, text.c_str(), caret);
    }

    void ImeBridge::select_all(const Textfield* field)
    {
        if (!active || field != focusedfield)
        {
            return;
        }

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onSelectAll)
            {
                window.MapleWasmIME.onSelectAll();
            }
        });
    }

    void ImeBridge::apply_text(const std::string& text, size_t caret_utf16)
    {
        if (active)
        {
            UI::get().ime_input(text, caret_utf16);
        }
    }

    void ImeBridge::apply_key(int32_t dom_keycode, bool pressed)
    {
        if (!active)
        {
            return;
        }

        int32_t key = dom_to_glfw(dom_keycode);
        if (key != 0)
        {
            UI::get().send_key(key, pressed);
        }
    }
}

extern "C"
{
    // Read-only state probe used by the browser acceptance flow. Keeping the
    // state in C++ avoids treating a timed click sequence as proof of success.
    EMSCRIPTEN_KEEPALIVE int msui_state()
    {
        jrc::UI& ui = jrc::UI::get();
        if (ui.is_element_active(jrc::UIElement::CASHSHOP)) return 6;
        if (ui.is_element_active(jrc::UIElement::STATUSBAR)) return 5;
        if (ui.is_element_active(jrc::UIElement::CHARCREATION)) return 4;
        if (ui.is_element_active(jrc::UIElement::CHARSELECT)) return 3;
        if (ui.is_element_active(jrc::UIElement::WORLDSELECT)) return 2;
        if (ui.is_element_active(jrc::UIElement::LOGIN)) return 1;
        return 0;
    }

    // A modal notice does not replace its parent UI state, so expose it
    // separately to let browser acceptance prove that an error was visible.
    EMSCRIPTEN_KEEPALIVE int msui_login_notice_active()
    {
        return jrc::UI::get().is_element_active(jrc::UILoginNotice::TYPE) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_login_notice_message()
    {
        auto notice = jrc::UI::get().get_element<jrc::UILoginNotice>();
        return notice ? static_cast<int>(notice->get_message()) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int msui_character_creation_customizing()
    {
        auto creation = jrc::UI::get().get_element<jrc::UICharcreation>();
        return creation && creation->is_customizing() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_notice_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::NOTICE) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_locker_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->locker_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_inventory_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->inventory_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_nx_credit()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->nx_credit() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_selected_right_row()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->selected_right_row() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_pending()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop && shop->is_pending() ? 1 : 0;
    }

    // Browser click compatibility entry point. UIWorldSelect makes this
    // idempotent with the normal GLFW mouse-down route.
    EMSCRIPTEN_KEEPALIVE int msworldselect_enter()
    {
        auto worldselect = jrc::UI::get().get_element<jrc::UIWorldSelect>();
        return worldselect && worldselect->enter_selected_channel() ? 1 : 0;
    }

    // Replace the focused field's text with the textarea content; the caret is
    // a UTF-16 offset, matching the browser's selectionStart.
    EMSCRIPTEN_KEEPALIVE void msime_input(const char* full_text, int caret_utf16)
    {
        jrc::ImeBridge::apply_text(
            full_text != nullptr ? full_text : "",
            caret_utf16 > 0 ? static_cast<size_t>(caret_utf16) : 0);
    }

    // Forward a whitelisted control key using its DOM key code.
    EMSCRIPTEN_KEEPALIVE void msime_key(int dom_keycode, int pressed)
    {
        jrc::ImeBridge::apply_key(dom_keycode, pressed != 0);
    }
}

#else

namespace jrc
{
    bool ImeBridge::is_active()
    {
        return false;
    }

    void ImeBridge::focus_field(Textfield*) {}

    void ImeBridge::blur_field() {}

    void ImeBridge::sync_field(const Textfield*) {}

    void ImeBridge::select_all(const Textfield*) {}

    void ImeBridge::apply_text(const std::string&, size_t) {}

    void ImeBridge::apply_key(int32_t, bool) {}
}

#endif
