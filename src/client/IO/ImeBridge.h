#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace jrc
{
    class Textfield;

    // Bridge between the browser's native input method and the focused in-game
    // textfield (WASM only; on other platforms every call is a no-op).
    //
    // The browser cannot route IME composition into a canvas, so while a
    // textfield is focused a hidden HTML textarea takes keyboard focus instead.
    // The textarea content is mirrored into the textfield through msime_input
    // and whitelisted control keys arrive through msime_key; the textfield
    // mirrors its own changes back through sync_field so both sides agree.
    namespace ImeBridge
    {
        // True while the hidden textarea owns keyboard focus.
        bool is_active();

        // Textfield hooks for focus changes and text updates.
        void focus_field(Textfield* field);
        void blur_field();
        void sync_field(const Textfield* field);

        // Entry points behind the exported WASM functions.
        void apply_text(const std::string& text, size_t caret_utf16);
        void apply_key(int32_t dom_keycode, bool pressed);
    }
}
