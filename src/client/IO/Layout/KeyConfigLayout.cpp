#include "KeyConfigLayout.h"

#include <initializer_list>

namespace jrc::KeyConfigLayout
{
    KeyBounds key_bounds()
    {
        KeyBounds bounds;

        auto add_key = [&](KeyConfig::Key key, int16_t x, int16_t y,
                           int16_t width = 32, int16_t height = 32)
        {
            bounds.emplace(
                key,
                Rectangle<int16_t>(
                    { x, y },
                    { static_cast<int16_t>(x + width), static_cast<int16_t>(y + height) }
                )
            );
        };

        auto add_row = [&](std::initializer_list<KeyConfig::Key> keys, int16_t start_x, int16_t y)
        {
            int16_t x = start_x;
            for (KeyConfig::Key key : keys)
            {
                add_key(key, x, y);
                x += 34;
            }
        };

        add_key(KeyConfig::ESCAPE, 0, 0);
        add_row({ KeyConfig::F1, KeyConfig::F2, KeyConfig::F3, KeyConfig::F4 }, 68, 0);
        add_row({ KeyConfig::F5, KeyConfig::F6, KeyConfig::F7, KeyConfig::F8 }, 212, 0);
        add_row({ KeyConfig::F9, KeyConfig::F10, KeyConfig::F11, KeyConfig::F12 }, 356, 0);
        add_key(KeyConfig::SCROLL_LOCK, 534, 0);

        add_key(KeyConfig::GRAVE_ACCENT, 0, 38);
        add_row(
            {
                KeyConfig::NUM1, KeyConfig::NUM2, KeyConfig::NUM3, KeyConfig::NUM4,
                KeyConfig::NUM5, KeyConfig::NUM6, KeyConfig::NUM7, KeyConfig::NUM8,
                KeyConfig::NUM9, KeyConfig::NUM0, KeyConfig::MINUS, KeyConfig::EQUAL
            },
            34,
            38
        );
        add_row({ KeyConfig::INSERT, KeyConfig::HOME, KeyConfig::PAGE_UP }, 500, 38);

        add_row(
            {
                KeyConfig::Q, KeyConfig::W, KeyConfig::E, KeyConfig::R,
                KeyConfig::T, KeyConfig::Y, KeyConfig::U, KeyConfig::I,
                KeyConfig::O, KeyConfig::P, KeyConfig::LEFT_BRACKET,
                KeyConfig::RIGHT_BRACKET, KeyConfig::BACKSLASH
            },
            50,
            71
        );
        add_row({ KeyConfig::DELETE, KeyConfig::END, KeyConfig::PAGE_DOWN }, 500, 71);

        add_row(
            {
                KeyConfig::A, KeyConfig::S, KeyConfig::D, KeyConfig::F,
                KeyConfig::G, KeyConfig::H, KeyConfig::J, KeyConfig::K,
                KeyConfig::L, KeyConfig::SEMICOLON, KeyConfig::APOSTROPHE
            },
            68,
            104
        );

        add_key(KeyConfig::LEFT_SHIFT, 0, 137, 84);
        add_row(
            {
                KeyConfig::Z, KeyConfig::X, KeyConfig::C, KeyConfig::V,
                KeyConfig::B, KeyConfig::N, KeyConfig::M, KeyConfig::COMMA,
                KeyConfig::PERIOD
            },
            84,
            137
        );
        add_key(KeyConfig::RIGHT_SHIFT, 424, 137, 68);

        add_key(KeyConfig::LEFT_CONTROL, 0, 170, 50);
        add_key(KeyConfig::LEFT_ALT, 100, 170, 54);
        add_key(KeyConfig::SPACE, 154, 170, 168);
        add_key(KeyConfig::RIGHT_ALT, 322, 170, 56);
        add_key(KeyConfig::RIGHT_CONTROL, 434, 170, 58);

        return bounds;
    }
}
