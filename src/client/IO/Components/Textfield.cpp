//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "Textfield.h"

#include "../ImeBridge.h"
#include "../UI.h"

#include "../../Constants.h"
#include "../../Util/Utf8.h"


namespace jrc
{
    Textfield::Textfield(Text::Font font,
                         Text::Alignment alignment,
                         Text::Color color,
                         Rectangle<int16_t> bnd,
                         size_t lim)
        : textlabel(font, alignment, color, "", 0, false), text(),
          marker(font, alignment, color, "|"),             markerpos(0),
          allselected(false),                              bounds(bnd),
          limit(lim),
          crypt(0),                                        state(NORMAL)
        {}

    Textfield::Textfield() = default;

    Textfield::~Textfield() = default;

    void Textfield::draw(Point<int16_t> parent) const
    {
        if (state == DISABLED)
        {
            return;
        }

        Point<int16_t> absp = bounds.getlt() + parent;
        if (text.size() > 0)
        {
            textlabel.draw(absp);
        }

        if (state == FOCUSED && showmarker)
        {
            Point<int16_t> mpos = absp + Point<int16_t>(textlabel.advance(markerpos), -1);
            marker.draw(mpos);
        }
    }

    void Textfield::update(Point<int16_t> parent)
    {
        if (state == DISABLED)
        {
            return;
        }

        parentpos = parent;

        elapsed += Constants::TIMESTEP;
        if (elapsed > 256)
        {
            showmarker = !showmarker;
            elapsed = 0;
        }
    }

    void Textfield::set_state(State st)
    {
        if (state != st)
        {
            State previous = state;
            state      = st;
            elapsed    = 0;
            showmarker = true;

            if (state == FOCUSED)
            {
                UI::get().focus_textfield(this);
                ImeBridge::focus_field(this);
            }
            else if (previous == FOCUSED)
            {
                allselected = false;
                UI::get().blur_textfield(this);
                ImeBridge::blur_field();
            }
        }
    }

    void Textfield::set_enter_callback(std::function<void(std::string)> on_return)
    {
        onreturn = on_return;
    }

    void Textfield::set_key_callback(KeyAction::Id key, std::function<void(void)> action)
    {
        callbacks[key] = action;
    }

    void Textfield::send_key(KeyType::Id type, int32_t key, bool pressed)
    {
        switch (type)
        {
        case KeyType::ACTION:
            if (pressed)
            {
                switch (key)
                {
                case KeyAction::LEFT:
                    if (allselected)
                    {
                        allselected = false;
                        markerpos = 0;
                        ImeBridge::sync_field(this);
                    }
                    else if (markerpos > 0)
                    {
                        // Step over the whole preceding codepoint, not just one
                        // byte of a multi-byte sequence.
                        markerpos--;
                        while (markerpos > 0 && Utf8::is_continuation(text[markerpos]))
                        {
                            markerpos--;
                        }
                    }
                    break;
                case KeyAction::RIGHT:
                    if (allselected)
                    {
                        allselected = false;
                        markerpos = text.size();
                        ImeBridge::sync_field(this);
                    }
                    else if (markerpos < text.size())
                    {
                        markerpos++;
                        while (markerpos < text.size() && Utf8::is_continuation(text[markerpos]))
                        {
                            markerpos++;
                        }
                    }
                    break;
                case KeyAction::BACK:
                    if (clear_selection())
                    {
                        modifytext(text);
                    }
                    else if (text.size() > 0 && markerpos > 0)
                    {
                        // Delete the entire codepoint before the caret.
                        size_t start = markerpos - 1;
                        while (start > 0 && Utf8::is_continuation(text[start]))
                        {
                            start--;
                        }
                        text.erase(start, markerpos - start);
                        markerpos = start;
                        modifytext(text);
                    }
                    break;
                case KeyAction::RETURN:
                    if (onreturn && text.size() > 0)
                    {
                        onreturn(text);
                        text = "";
                        markerpos = 0;
                        modifytext(text);
                    }
                    else if (callbacks.count(KeyAction::RETURN))
                    {
                        callbacks.at(KeyAction::RETURN)();
                    }
                    break;
                case KeyAction::SPACE:
                {
                    bool replaced = clear_selection();
                    if (markerpos > 0 && belowlimit())
                    {
                        text.insert(markerpos, 1, ' ');
                        markerpos++;
                        modifytext(text);
                    }
                    else if (replaced)
                    {
                        modifytext(text);
                    }
                    break;
                }
                default:
                    if (callbacks.count(key))
                    {
                        callbacks.at(key)();
                    }
                    break;
                }
            }
            break;
        case KeyType::LETTER:
        case KeyType::NUMBER:
            if (!pressed)
            {
                auto c = static_cast<int8_t>(key);
                bool replaced = clear_selection();
                if (belowlimit())
                {
                    text.insert(markerpos, 1, c);
                    markerpos++;
                    modifytext(text);
                }
                else if (replaced)
                {
                    modifytext(text);
                }
            }
            break;
        default:
            break;
        }
    }

    void Textfield::add_string(const std::string& str)
    {
        bool replaced = !str.empty() && clear_selection();
        bool inserted = false;

        for (size_t i = 0; i < str.size(); )
        {
            size_t seqlen = Utf8::sequence_length(str[i]);
            if (i + seqlen > str.size())
            {
                seqlen = str.size() - i;
            }

            if (!belowlimit(seqlen))
            {
                break;
            }

            text.insert(markerpos, str, i, seqlen);
            markerpos += seqlen;
            inserted = true;

            i += seqlen;
        }

        if (replaced || inserted)
        {
            modifytext(text);
        }
    }

    void Textfield::modifytext(const std::string& t)
    {
        if (crypt > 0)
        {
            std::string crypted;
            crypted.insert(0, t.size(), crypt);
            textlabel.change_text(crypted);
        }
        else
        {
            textlabel.change_text(t);
        }

        text = t;

        ImeBridge::sync_field(this);
    }

    Cursor::State Textfield::send_cursor(Point<int16_t> cursorpos, bool clicked)
    {
        if (state == DISABLED)
        {
            return Cursor::IDLE;
        }

        auto abs_bounds = get_bounds();
        if (abs_bounds.contains(cursorpos))
        {
            if (clicked)
            {
                switch (state)
                {
                case NORMAL:
                    set_state(FOCUSED);
                    break;
                default:
                    break;
                }
                return Cursor::CLICKING;
            }
            else
            {
                return Cursor::CANCLICK;
            }
        }
        else
        {
            if (clicked)
            {
                switch (state)
                {
                case FOCUSED:
                    set_state(NORMAL);
                    break;
                default:
                    break;
                }
            }
            return Cursor::IDLE;
        }
    }

    void Textfield::change_text(const std::string& t)
    {
        allselected = false;
        markerpos = t.size();
        modifytext(t);
    }

    void Textfield::select_all()
    {
        if (text.empty())
        {
            return;
        }

        allselected = true;
        markerpos = text.size();
        ImeBridge::select_all(this);
    }

    void Textfield::set_text_with_caret(const std::string& newtext, size_t caret_utf16)
    {
        allselected = false;
        std::string value = newtext;

        // Fixed-limit fields are bounded by the protocol in bytes, so trim at
        // the largest codepoint boundary that still fits.
        if (limit > 0 && value.size() > limit)
        {
            size_t cut = limit;
            while (cut > 0 && Utf8::is_continuation(value[cut]))
            {
                cut--;
            }
            value.resize(cut);
        }

        size_t caret = Utf8::utf16_to_byte_offset(value, caret_utf16);

        // Update the caret before modifytext() echoes the field to the IME
        // bridge: echoing the stale caret makes the browser re-sync the same
        // text with the other caret, which ping-pongs forever.
        markerpos = caret;

        modifytext(value);

        if (limit == 0)
        {
            // Width-limited field: drop trailing codepoints until it fits.
            while (!text.empty() && !belowlimit(0))
            {
                size_t last = text.size() - 1;
                while (last > 0 && Utf8::is_continuation(text[last]))
                {
                    last--;
                }
                text.erase(last);
                modifytext(text);
            }
        }

        if (caret > text.size())
        {
            caret = text.size();
        }
        markerpos = caret;
    }

    bool Textfield::clear_selection()
    {
        if (!allselected)
        {
            return false;
        }

        text.clear();
        markerpos = 0;
        allselected = false;
        return true;
    }

    void Textfield::set_cryptchar(int8_t c)
    {
        crypt = c;
    }

    bool Textfield::belowlimit(size_t extra) const
    {
        if (limit > 0)
        {
            return text.size() + extra <= limit;
        }
        else
        {
            uint16_t advance = textlabel.advance(text.size());
            return (advance + 50) < bounds.get_horizontal().length();
        }
    }

    const std::string& Textfield::get_text() const
    {
        return text;
    }

    bool Textfield::is_crypted() const
    {
        return crypt > 0;
    }

    size_t Textfield::caret_utf16() const
    {
        return Utf8::byte_to_utf16_offset(text, markerpos);
    }

    bool Textfield::empty() const
    {
        return text.empty();
    }

    Textfield::State Textfield::get_state() const
    {
        return state;
    }

    Rectangle<int16_t> Textfield::get_bounds() const
    {
        return Rectangle<int16_t>(
            bounds.getlt() + parentpos,
            bounds.getrb() + parentpos
        );
    }
}
