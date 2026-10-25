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
// Pure model of the quickslot display: eight fixed slots in a 4x2 grid.
// Two asset generations ship different panels, so the measured geometry
// lives in Layout values (V83_LAYOUT / CHAOS_LAYOUT) and every helper
// takes the active layout as an argument. Deliberately free of graphics
// so hit-testing and assignment can be tested standalone.
#pragma once
#include "../../../Template/Point.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace jrc::quickslot
{
    /// Number of fixed slots in the v83 quickslot panel.
    constexpr int16_t COUNT = 8;

    /// Measured panel geometry of one asset generation. Low edges are
    /// inclusive, high edges exclusive, and the hit bands sit inside the
    /// visible slot boxes (separator gaps count towards the slot on their
    /// left so adjacent slots share edges without holes).
    struct Layout
    {
        /// Panel size in pixels (v83: 151x80, post-Chaos: 145x93).
        Point<int16_t> dimension;
        /// Inclusive/exclusive hit bands, local to the panel.
        std::array<int16_t, 4> column_left;
        std::array<int16_t, 4> column_right;
        std::array<int16_t, 2> row_top;
        std::array<int16_t, 2> row_bottom;
        /// X center of each slot column, local to the panel.
        std::array<int16_t, 4> column_center;
        /// Y center of each slot row, local to the panel.
        std::array<int16_t, 2> row_center;
        /// Side length of the square cooldown cover, matched to the slots.
        int16_t cover_size;
        /// Keycap label center Y per row when the labels sit in panel chrome
        /// above/below the slot grid instead of inside empty slots.
        std::array<int16_t, 2> key_label_y;
        /// Whether keycap labels are always drawn at `key_label_y` (the
        /// post-Chaos panel has header/base bands for them). When false the
        /// labels are drawn only on empty slots at `row_center`.
        bool labels_outside_slots;
    };

    /// v83 panel (StatusBar.img/base/quickSlot, 151x80).
    constexpr Layout V83_LAYOUT = {
        { 151, 80 },
        {   3,  37,  71, 105 }, {  37,  71, 105, 141 },
        {   7,  42 },            {  42,  77 },
        {  20,  54,  87, 124 },  {  24,  59 },
        32,                      {   0,   0 }, false
    };

    /// Post-Chaos panel (StatusBar2.img/mainBar/quickSlot/quickSlot,
    /// 145x93: 16px header band, two 30px slot rows, 14px base band).
    constexpr Layout CHAOS_LAYOUT = {
        { 145, 93 },
        {   8,  38,  71, 104 }, {  38,  71, 104, 138 },
        {  16,  46 },            {  46,  79 },
        {  23,  56,  89, 122 },  {  31,  64 },
        30,                      {   8,  86 }, true
    };

    struct State
    {
        /// Skill id per slot, 0 when empty or holding an item.
        std::array<int32_t, COUNT> skills{};
        /// Item id per slot, 0 when empty or holding a skill. At most one
        /// of `skills[i]` / `items[i]` is non-zero.
        std::array<int32_t, COUNT> items{};
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
    inline int16_t index_at(const Layout& layout, Point<int16_t> local_cursor)
    {
        const int16_t column = band_index(local_cursor.x(), layout.column_left, layout.column_right);
        if (column < 0)
        {
            return -1;
        }

        const int16_t row = band_index(local_cursor.y(), layout.row_top, layout.row_bottom);
        if (row < 0)
        {
            return -1;
        }

        return static_cast<int16_t>(row * 4 + column);
    }

    /// Center of slot `index` (0..7), local to the panel.
    inline Point<int16_t> key_center(const Layout& layout, int16_t index)
    {
        if (!valid_index(index))
        {
            return {};
        }
        return {
            layout.column_center[static_cast<size_t>(index % 4)],
            layout.row_center[static_cast<size_t>(index / 4)]
        };
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

    /// Assigned item id of a slot, 0 when empty or out of range.
    inline int32_t item_at(const State& state, int16_t index)
    {
        if (!valid_index(index))
        {
            return 0;
        }
        return state.items[static_cast<size_t>(index)];
    }

    /// Assign a skill to a slot; false for a bad index or skill id.
    inline bool assign(State& state, int16_t index, int32_t skill_id)
    {
        if (!valid_index(index) || skill_id <= 0)
        {
            return false;
        }
        const size_t slot = static_cast<size_t>(index);
        state.skills[slot] = skill_id;
        state.items[slot] = 0;
        return true;
    }

    /// Assign a use/setup item to a slot; false for a bad index or item id.
    inline bool assign_item(State& state, int16_t index, int32_t item_id)
    {
        if (!valid_index(index) || item_id <= 0)
        {
            return false;
        }
        const size_t slot = static_cast<size_t>(index);
        state.skills[slot] = 0;
        state.items[slot] = item_id;
        return true;
    }

    /// Empty a slot.
    inline void clear(State& state, int16_t index)
    {
        if (valid_index(index))
        {
            const size_t slot = static_cast<size_t>(index);
            state.skills[slot] = 0;
            state.items[slot] = 0;
        }
    }
}
