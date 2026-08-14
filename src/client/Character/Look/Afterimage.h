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
#include "Afterimage/Playback.h"

#include "../../Graphics/Animation.h"
#include "../../Template/Rectangle.h"

#include <vector>

namespace jrc
{
    class Afterimage
    {
    public:
        enum class Preparation
        {
            TRANSIENT_EFFECT,
            MAP_REQUIRED
        };

        Afterimage(int32_t skill_id, const std::string& name,
            const std::string& stance, int16_t level,
            Preparation preparation = Preparation::TRANSIENT_EFFECT);
        Afterimage();

        void draw(uint8_t stframe, const DrawArgument& args, float alpha) const;
        void update(uint8_t stframe, uint16_t timestep);

        uint8_t get_first_frame() const;
        Rectangle<int16_t> get_range() const;

    private:
        struct Cue
        {
            Animation animation;
            uint8_t firstframe;
            afterimage::PlaybackPhase phase;
        };

        std::vector<Cue> cues;
        Rectangle<int16_t> range;
        uint8_t firstframe;
    };
}
