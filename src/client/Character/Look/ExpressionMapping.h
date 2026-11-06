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
#include "Face.h"

#include <cstddef>
#include <cstdint>

namespace jrc
{
    namespace faceemote
    {
        // FACE1(100)..FACE7(106) are v83 wire emotes 1..7.
        inline int32_t from_action(size_t action)
        {
            const int32_t emote = static_cast<int32_t>(action) - 99;
            return emote >= 1 && emote <= 7 ? emote : 0;
        }

        // Wire 1..7 are the basic F-key faces (HIT..STUNNED). Wire 8..17
        // correspond to cash emote items 5160000..5160009.
        inline Expression::Id from_wire(int32_t emote)
        {
            if (emote >= 1 && emote <= 7)
                return static_cast<Expression::Id>(emote + 1);

            switch (emote)
            {
            case 0:  return Expression::DEFAULT;
            case 8:  return Expression::VOMIT;
            case 9:  return Expression::HUM;
            case 10: return Expression::CHEERS;
            case 11: return Expression::CHU;
            case 12: return Expression::WINK;
            case 13: return Expression::PAIN;
            case 14: return Expression::SHINE;
            case 15: return Expression::BLAZE;
            case 16: return Expression::GLITTER;
            case 17: return Expression::LOVE;
            default: return Expression::DEFAULT;
            }
        }
    }
}
