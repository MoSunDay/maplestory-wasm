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
#include "UIProgressBanner.h"

#include "../../Constants.h"

namespace jrc
{
    UIProgressBanner::UIProgressBanner(const std::string& str)
        : UIElement(),
          message(Text::A12M, Text::CENTER, Text::YELLOW, str),
          elapsed_ms(0)
    {
        opacity.set(1.0f);
    }

    Point<int16_t> UIProgressBanner::band_position() const
    {
        return Point<int16_t>(0, -Constants::VIEWYOFFSET);
    }

    void UIProgressBanner::draw(float alpha) const
    {
        const float interopc = opacity.get(alpha);
        if (interopc <= 0.0f)
        {
            return;
        }

        ColorBox band(
            Constants::viewwidth(),
            HEIGHT,
            Geometry::BLACK,
            BAND_OPACITY * interopc
        );
        band.draw(band_position());

        const int16_t x = static_cast<int16_t>(Constants::viewwidth() / 2);
        message.draw({ Point<int16_t>(x, -Constants::VIEWYOFFSET + 3), interopc });
    }

    void UIProgressBanner::update()
    {
        if (elapsed_ms < HOLD_DURATION)
        {
            elapsed_ms += Constants::TIMESTEP;
            return;
        }

        constexpr float FADE_STEP =
            Constants::TIMESTEP * 1.0f / FADE_DURATION;

        opacity -= FADE_STEP;
        if (opacity.last() < FADE_STEP)
        {
            deactivate();
        }
    }

    void UIProgressBanner::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            deactivate();
        }
    }
}
