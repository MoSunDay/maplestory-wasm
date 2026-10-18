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
#include "MapleButton.h"

namespace jrc
{
    namespace
    {
        // Post-Chaos files name the button states, pre-Chaos files number
        // them directly (0 = normal, 1 = pressed, 2 = mouseOver,
        // 3 = disabled).
        const char* named_state(Button::State state)
        {
            switch (state)
            {
            case Button::NORMAL:
                return "normal";
            case Button::PRESSED:
                return "pressed";
            case Button::MOUSEOVER:
                return "mouseOver";
            case Button::DISABLED:
                return "disabled";
            default:
                return "";
            }
        }

        // Numbered child used for the state in pre-Chaos button nodes.
        int numbered_state(Button::State state)
        {
            switch (state)
            {
            case Button::PRESSED:
                return 1;
            case Button::MOUSEOVER:
                return 2;
            case Button::DISABLED:
                return 3;
            default:
                return 0;
            }
        }

        // A state node keeps its bitmap in a numbered frame child ("0") so
        // animated states can hold several frames. Some pre-Chaos states
        // store the bitmap on the node itself instead.
        nl::node state_frame(nl::node state)
        {
            nl::node frame = state["0"];
            return frame ? frame : state;
        }

        // Named states take precedence so post-Chaos files resolve exactly
        // as before; numbered states are the pre-Chaos fallback.
        nl::node find_state(nl::node src, Button::State state)
        {
            nl::node named = src[named_state(state)];
            if (named)
                return state_frame(named);

            return state_frame(src[numbered_state(state)]);
        }
    }

    MapleButton::MapleButton(nl::node src, Point<int16_t> pos)
    {
        textures[NORMAL] = find_state(src, NORMAL);
        textures[PRESSED] = find_state(src, PRESSED);
        textures[MOUSEOVER] = find_state(src, MOUSEOVER);
        textures[DISABLED] = find_state(src, DISABLED);

        // Not every generation ships all four states. Fall back to NORMAL
        // so the button keeps working with the textures it does have.
        if (textures[NORMAL].is_valid())
        {
            if (!textures[PRESSED].is_valid())
                textures[PRESSED] = textures[NORMAL];
            if (!textures[MOUSEOVER].is_valid())
                textures[MOUSEOVER] = textures[NORMAL];
            if (!textures[DISABLED].is_valid())
                textures[DISABLED] = textures[NORMAL];
        }
        else
        {
            // Reverse fallback: keep the button visible when only NORMAL is
            // missing.
            if (textures[PRESSED].is_valid())
                textures[NORMAL] = textures[PRESSED];
            else if (textures[MOUSEOVER].is_valid())
                textures[NORMAL] = textures[MOUSEOVER];
            else if (textures[DISABLED].is_valid())
                textures[NORMAL] = textures[DISABLED];
        }

        position = pos;
        state = NORMAL;
        active = true;
    }

    MapleButton::MapleButton(nl::node src, int16_t x, int16_t y)
        : MapleButton(src, Point<int16_t>(x, y)) {}

    MapleButton::MapleButton(nl::node src)
        : MapleButton(src, Point<int16_t>()) {}

    void MapleButton::draw(Point<int16_t> parentpos) const
    {
        if (active)
        {
            // Anchor all states to the NORMAL state's origin to avoid visual
            // "teleporting" when state textures have different origins.
            Point<int16_t> normal_origin = textures[NORMAL].get_origin();
            Point<int16_t> state_origin = textures[state].get_origin();
            textures[state].draw(position + parentpos + state_origin - normal_origin);
        }
    }

    Rectangle<int16_t> MapleButton::bounds(Point<int16_t> parentpos) const
    {
        // Keep hit-testing stable across states to prevent hover/click flicker
        // when state textures differ in origin/dimensions.
        auto lt = parentpos + position - textures[NORMAL].get_origin();
        auto rb = lt + textures[NORMAL].get_dimensions();
        return Rectangle<int16_t>(lt, rb);
    }
}
