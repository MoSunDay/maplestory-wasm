//////////////////////////////////////////////////////////////////////////////
// Key hit boxes for the pre-Chaos (v83) key configuration window.
//
// The v83 bitmap (UIWindow.img/KeyConfig/backgrnd, 629x373) paints the
// keyboard with 32px key caps on a 34px column pitch.  The rows sit at
// y = 27, 66, 99, 132, 165 and 198; wide keys (shift, space, ...) use the
// width of their painted cap.  Keys without a KeyConfig::Id (tab, caps
// lock, enter, backspace, slash, the Korean toggle keys and the arrow
// keys) are not bindable and are left out.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../KeyConfig.h"
#include "../../Template/Rectangle.h"

#include <map>

namespace jrc::KeyConfigV83Layout
{
    using KeyBounds = std::map<KeyConfig::Key, Rectangle<int16_t>>;

    inline KeyBounds key_bounds()
    {
        KeyBounds bounds;

        auto add_key = [](KeyBounds& out, KeyConfig::Key key, int16_t x,
                          int16_t y, int16_t width = 32, int16_t height = 32) {
            out.emplace(
                key,
                Rectangle<int16_t>(
                    { x, y },
                    { static_cast<int16_t>(x + width),
                      static_cast<int16_t>(y + height) }
                )
            );
        };

        // Function row (y = 27).  Scroll lock is the labelled middle key of
        // the three-key cluster at the right end of the row.
        add_key(bounds, KeyConfig::ESCAPE, 14, 27);
        add_key(bounds, KeyConfig::F1, 46, 27);
        add_key(bounds, KeyConfig::F2, 83, 27);
        add_key(bounds, KeyConfig::F3, 117, 27);
        add_key(bounds, KeyConfig::F4, 151, 27);
        add_key(bounds, KeyConfig::F5, 185, 27);
        add_key(bounds, KeyConfig::F6, 228, 27);
        add_key(bounds, KeyConfig::F7, 261, 27);
        add_key(bounds, KeyConfig::F8, 296, 27);
        add_key(bounds, KeyConfig::F9, 330, 27);
        add_key(bounds, KeyConfig::F10, 371, 27);
        add_key(bounds, KeyConfig::F11, 405, 27);
        add_key(bounds, KeyConfig::F12, 440, 27);
        add_key(bounds, KeyConfig::SCROLL_LOCK, 548, 27);

        // Number row (y = 66), followed by insert/home/page up.
        add_key(bounds, KeyConfig::GRAVE_ACCENT, 14, 66);
        add_key(bounds, KeyConfig::NUM1, 50, 66);
        add_key(bounds, KeyConfig::NUM2, 84, 66);
        add_key(bounds, KeyConfig::NUM3, 118, 66);
        add_key(bounds, KeyConfig::NUM4, 152, 66);
        add_key(bounds, KeyConfig::NUM5, 187, 66);
        add_key(bounds, KeyConfig::NUM6, 220, 66);
        add_key(bounds, KeyConfig::NUM7, 254, 66);
        add_key(bounds, KeyConfig::NUM8, 288, 66);
        add_key(bounds, KeyConfig::NUM9, 322, 66);
        add_key(bounds, KeyConfig::NUM0, 356, 66);
        add_key(bounds, KeyConfig::MINUS, 390, 66);
        add_key(bounds, KeyConfig::EQUAL, 425, 66);
        add_key(bounds, KeyConfig::INSERT, 517, 66);
        add_key(bounds, KeyConfig::HOME, 551, 66);
        add_key(bounds, KeyConfig::PAGE_UP, 584, 66);

        // QWERTY row (y = 99), followed by delete/end/page down.  The v83
        // bitmap ends the row at the bracket keys; there is no painted
        // backslash key, so that id stays unbindable here.
        add_key(bounds, KeyConfig::Q, 100, 99);
        add_key(bounds, KeyConfig::W, 135, 99);
        add_key(bounds, KeyConfig::E, 169, 99);
        add_key(bounds, KeyConfig::R, 202, 99);
        add_key(bounds, KeyConfig::T, 236, 99);
        add_key(bounds, KeyConfig::Y, 271, 99);
        add_key(bounds, KeyConfig::U, 304, 99);
        add_key(bounds, KeyConfig::I, 338, 99);
        add_key(bounds, KeyConfig::O, 372, 99);
        add_key(bounds, KeyConfig::P, 407, 99);
        add_key(bounds, KeyConfig::LEFT_BRACKET, 440, 99);
        add_key(bounds, KeyConfig::RIGHT_BRACKET, 472, 99, 34);
        add_key(bounds, KeyConfig::DELETE, 517, 99);
        add_key(bounds, KeyConfig::END, 551, 99);
        add_key(bounds, KeyConfig::PAGE_DOWN, 584, 99);

        // Home row (y = 132).
        add_key(bounds, KeyConfig::A, 83, 132);
        add_key(bounds, KeyConfig::S, 117, 132);
        add_key(bounds, KeyConfig::D, 151, 132);
        add_key(bounds, KeyConfig::F, 185, 132);
        add_key(bounds, KeyConfig::G, 220, 132);
        add_key(bounds, KeyConfig::H, 253, 132);
        add_key(bounds, KeyConfig::J, 287, 132);
        add_key(bounds, KeyConfig::K, 321, 132);
        add_key(bounds, KeyConfig::L, 355, 132);
        add_key(bounds, KeyConfig::SEMICOLON, 389, 132);
        add_key(bounds, KeyConfig::APOSTROPHE, 423, 132);

        // Bottom letter row (y = 165) with wide shift keys.
        add_key(bounds, KeyConfig::LEFT_SHIFT, 14, 165, 82);
        add_key(bounds, KeyConfig::Z, 100, 165);
        add_key(bounds, KeyConfig::X, 134, 165);
        add_key(bounds, KeyConfig::C, 168, 165);
        add_key(bounds, KeyConfig::V, 202, 165);
        add_key(bounds, KeyConfig::B, 237, 165);
        add_key(bounds, KeyConfig::N, 270, 165);
        add_key(bounds, KeyConfig::M, 304, 165);
        add_key(bounds, KeyConfig::COMMA, 338, 165);
        add_key(bounds, KeyConfig::PERIOD, 372, 165);
        add_key(bounds, KeyConfig::RIGHT_SHIFT, 439, 165, 64);

        // Modifier row (y = 198).  Between control and alt the bitmap
        // paints the Korean hangul/hanja keys, which have no id here.
        add_key(bounds, KeyConfig::LEFT_CONTROL, 14, 198, 46);
        add_key(bounds, KeyConfig::LEFT_ALT, 114, 198, 50);
        add_key(bounds, KeyConfig::SPACE, 166, 198, 167);
        add_key(bounds, KeyConfig::RIGHT_ALT, 336, 198, 53);
        add_key(bounds, KeyConfig::RIGHT_CONTROL, 448, 198, 55);

        return bounds;
    }
}
