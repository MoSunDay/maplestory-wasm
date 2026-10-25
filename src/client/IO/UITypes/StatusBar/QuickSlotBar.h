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
// Drawing of the v83 and post-Chaos quickslot displays; the model itself
// lives in QuickSlotModel.h.
#pragma once
#include "QuickSlotModel.h"
#include "../../../Graphics/Text.h"
#include "../../../Graphics/Texture.h"

#include <array>

namespace jrc::quickslot
{
    /// Draw the panel, its eight slots (skills/items) and keycap labels at
    /// `origin` (absolute panel top-left). `layout` supplies the
    /// generation's measured geometry, `cooldown_labels` the remaining
    /// seconds per skill slot. Returns the panel's exact screen destination
    /// rectangle captured from that same panel draw call.
    Rectangle<int16_t> draw(const Layout& layout, const State& state, const Texture& panel,
        const Texture (&keys)[COUNT],
        const std::array<Text, COUNT>& cooldown_labels,
        Point<int16_t> origin);
}
