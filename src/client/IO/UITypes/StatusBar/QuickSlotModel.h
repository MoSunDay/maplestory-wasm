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
// Pure model of the pre-Chaos (v83) quickslot display
// (StatusBar.img/base/quickSlot, 151x80, eight fixed slots). Deliberately
// free of graphics so hit-testing and assignment can be tested standalone.
#pragma once
#include "../../../Template/Point.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace jrc::quickslot
{
    /// Number of fixed slots in the v83 quickslot panel.
    constexpr int16_t COUNT = 8;

    /// Slot hit bands measured from base/quickSlot (151x80), local to the
    /// panel: four columns and two rows. Low edges are inclusive, high edges
    /// exclusive, and the bands sit inside the visible keycaps.
    constexpr std::array<int16_t, 4> COLUMN_LEFT  = {   3,  37,  71, 105 };
    constexpr std::array<int16_t, 4> COLUMN_RIGHT = {  37,  71, 105, 141 };
    constexpr std::array<int16_t, 2> ROW_TOP      = {   7,  42 };
    constexpr std::array<int16_t, 2> ROW_BOTTOM   = {  42,  77 };

    struct State
    {
        /// Skill id per slot, 0 for an empty slot (key label shown).
        std::array<int32_t, COUNT> skills{};
        /// Slot under the cursor, -1 when outside the panel.
        int16_t hovered = -1;
        /// Cursor press id of the last handled press, so one click (or a
        /// held button) cannot trigger a slot more than once.
        uint64_t press_id = 0;
    };

    /// Index of the band containing `value`, -1 when outside every band.
    template <size_t N>
    inline int16_t band_index(
        int16_t value,
        const std::array<int16_t, N>& lows,
        const std::array<int16_t, N>& highs
    )
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (value >= lows[i] && value < highs[i])
            {
                return static_cast<int16_t>(i);
            }
        }
        return -1;
    }

    /// Whether `index` addresses one of the COUNT slots.
    inline bool valid_index(int16_t index)
    {
        return index >= 0 && index < COUNT;
    }

    /// Slot index for a cursor position local to the panel, -1 if outside.
    inline int16_t index_at(Point<int16_t> local_cursor)
    {
        const int16_t column = band_index(local_cursor.x(), COLUMN_LEFT, COLUMN_RIGHT);
        if (column < 0)
        {
            return -1;
        }

        const int16_t row = band_index(local_cursor.y(), ROW_TOP, ROW_BOTTOM);
        if (row < 0)
        {
            return -1;
        }

        return static_cast<int16_t>(row * 4 + column);
    }

    /// Assigned skill id of a slot, 0 when empty or out of range.
    inline int32_t skill_at(const State& state, int16_t index)
    {
        if (!valid_index(index))
        {
            return 0;
        }
        return state.skills[static_cast<size_t>(index)];
    }

    /// Assign a skill to a slot; false for a bad index or skill id.
    inline bool assign(State& state, int16_t index, int32_t skill_id)
    {
        if (!valid_index(index) || skill_id <= 0)
        {
            return false;
        }
        state.skills[static_cast<size_t>(index)] = skill_id;
        return true;
    }

    /// Empty a slot.
    inline void clear(State& state, int16_t index)
    {
        if (valid_index(index))
        {
            state.skills[static_cast<size_t>(index)] = 0;
        }
    }
}
