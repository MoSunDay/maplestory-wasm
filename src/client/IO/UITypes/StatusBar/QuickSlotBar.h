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
// Drawing of the pre-Chaos (v83) quickslot display; the model itself lives
// in QuickSlotModel.h.
#pragma once
#include "QuickSlotModel.h"
#include "../../../Graphics/Text.h"
#include "../../../Graphics/Texture.h"

#include <array>

namespace jrc::quickslot
{
    /// Draw the panel and its eight slots at `origin` (absolute panel
    /// top-left). `cooldown_labels` holds the remaining seconds per slot.
    void draw(const State& state, const Texture& panel,
        const Texture (&keys)[COUNT],
        const std::array<Text, COUNT>& cooldown_labels,
        Point<int16_t> origin);
}
