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
#pragma once
#include "../UIElement.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include "../../Template/Interpolated.h"

#include <string>

namespace jrc
{
    // A banner across the top of the screen showing script progress messages
    // (opcode 122). Re-emplacing replaces the previous message.
    class UIProgressBanner : public UIElement
    {
    public:
        static constexpr Type TYPE = PROGRESSBANNER;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        explicit UIProgressBanner(const std::string& message);

        void draw(float alpha) const override;
        void update() override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

    private:
        Point<int16_t> band_position() const;

        Text message;
        Linear<float> opacity;
        int64_t elapsed_ms;

        static constexpr int16_t HEIGHT = 24;
        static constexpr float BAND_OPACITY = 0.6f;
        // Hold the banner fully visible, then fade it out.
        static constexpr int64_t HOLD_DURATION = 1'000;
        static constexpr int64_t FADE_DURATION = 5'000;
    };
}
