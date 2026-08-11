#include "ImeBridge.h"

#include "Components/Textfield.h"
#include "UI.h"

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

    void ImeBridge::apply_text(const std::string&, size_t) {}

    void ImeBridge::apply_key(int32_t, bool) {}
}

#endif
