//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//////////////////////////////////////////////////////////////////////////////
// Free-function coordinate vocabulary for the pre-Chaos (v83) status bar.
//
// v83 ships `StatusBar.img` instead of `StatusBar2.img`: the bar is a
// 1280x71 strip anchored to the BOTTOM-LEFT of the view (the post-Chaos
// bar is centered and 1366 wide). Every HUD element of the status bar,
// the chat bar and the menu popup derives its position from here so no
// magic numbers are scattered through the element code.
//
// Layout metrics below were measured from the actual UI.nx textures:
//   base/backgrnd      1280x71   light grey full-width strip
//   base/quickSlot      151x80   4x2 key display (9px taller than the bar)
//   gauge/bar           340x31   "HP"/"MP"/"EXP" labels above the fills
//   gauge/graduation    340x31   frame + ticks + translucent dark base
//   base/chat            566x5   chat log drag strip
//   base/chatTarget       81x20  chat target selector box
//   UIWindow.img/ContextMenu t/c/s  menu popup frame pieces
#pragma once
#include "../../../Constants.h"
#include "../../UiAssetVersion.h"
#include "../../../Template/Point.h"
#include "../../../Template/Rectangle.h"

#include <algorithm>

namespace jrc
{
    namespace statusbar
    {
        /// Which of the three v83 gauges a coordinate refers to.
        enum class Gauge
        {
            HP,
            MP,
            EXP
        };

        // ------------------------------------------------------------------
        // Generation detection (same marker convention as UiAssetVersion.h)
        // ------------------------------------------------------------------

        /// True when the loaded UI.nx is the pre-Chaos (v83) asset set, i.e.
        /// when StatusBar2.img (and its whole node family) is absent.
        inline bool prechaos()
        {
            const bool post_chaos = static_cast<bool>(
                nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"]);
            return !post_chaos;
        }

        // ------------------------------------------------------------------
        // Bar geometry
        // ------------------------------------------------------------------

        /// Height of the v83 bottom strip (base/backgrnd).
        constexpr int16_t BAR_HEIGHT = 71;
        /// Width of the v83 bottom strip.
        constexpr int16_t BAR_WIDTH = 1280;
        /// Size of the quickslot display (base/quickSlot).
        constexpr int16_t QUICKSLOT_WIDTH = 151;
        constexpr int16_t QUICKSLOT_HEIGHT = 80;

        /// Element anchor of the status bar. The v83 bar hugs the bottom-left
        /// corner of the view; the post-Chaos bar keeps its centered anchor.
        inline Point<int16_t> bar_position()
        {
            return { 0, static_cast<int16_t>(
                Constants::viewheight() - Constants::VIEWYOFFSET - BAR_HEIGHT
            ) };
        }

        /// Element dimension matching the shipped bar texture.
        inline Point<int16_t> bar_dimension()
        {
            return {
                static_cast<int16_t>(Constants::viewwidth()),
                BAR_HEIGHT
            };
        }

        /// Hit-test bounds of the whole HUD strip for the current view.
        inline Rectangle<int16_t> statusbar_range(Point<int16_t> position)
        {
            const int16_t width = static_cast<int16_t>(
                std::min<int32_t>(Constants::viewwidth(), BAR_WIDTH)
            );
            return { position, position + Point<int16_t>(width, BAR_HEIGHT) };
        }

        // ------------------------------------------------------------------
        // Gauge block (gauge/bar + gauge/graduation, 340x31)
        // ------------------------------------------------------------------

        /// Top-left corner of the 340x31 gauge block, relative to the bar.
        constexpr Point<int16_t> gauge_anchor()
        {
            return { 6, 20 };
        }

        /// Inner fill rectangle of one gauge slot, relative to the block.
        /// Derived from the frame borders in gauge/graduation.
        inline Rectangle<int16_t> gauge_fill(Gauge kind)
        {
            switch (kind)
            {
            case Gauge::HP:
                return { 3, 103, 15, 29 };
            case Gauge::MP:
                return { 110, 208, 15, 29 };
            case Gauge::EXP:
            default:
                return { 214, 338, 15, 29 };
            }
        }

        /// Right edge for the number charset drawn after a gauge slot.
        constexpr int16_t gauge_number_right(Gauge kind)
        {
            return kind == Gauge::HP ? 109
                 : kind == Gauge::MP ? 211
                 : 346;
        }

        /// Vertical anchor of the gauge number row (5-9px glyphs).
        constexpr int16_t gauge_number_y()
        {
            return 17;
        }

        /// Opacity of the box covering the unfilled part of a gauge. The v83
        /// empty slot shows the dark base of gauge/graduation.
        constexpr float gauge_empty_opacity()
        {
            return 0.74f;
        }

        // ------------------------------------------------------------------
        // Level / name / job labels on the free top band of the bar
        // ------------------------------------------------------------------

        constexpr Point<int16_t> level_pos()  { return { 8, 4 };  }
        constexpr Point<int16_t> name_pos()   { return { 34, 4 }; }
        constexpr Point<int16_t> job_pos()    { return { 8, 16 }; }

        // ------------------------------------------------------------------
        // Chat row (bottom band of the bar)
        // ------------------------------------------------------------------

        /// Visible width of the chat log / input row (base/chat is 566px).
        constexpr int16_t chat_log_width()
        {
            return 566;
        }

        /// Top-left of the 5px chat drag strip (base/chat).
        constexpr Point<int16_t> chat_strip_pos()
        {
            return { 0, 49 };
        }

        /// Top-left of the chat target box (base/chatTarget, 81x20).
        constexpr Point<int16_t> chat_target_pos()
        {
            return { 4, 50 };
        }

        /// X of the three big 54x34 buttons right of the quickslot panel.
        /// index 0 = BtShort, 1 = BtShop, 2 = BtMenu (rightmost).
        inline int16_t big_button_x(int16_t index)
        {
            return static_cast<int16_t>(
                Constants::viewwidth() - QUICKSLOT_WIDTH - 58 - index * 56
            );
        }

        /// Input field rectangle, relative to the bar anchor. The right edge
        /// keeps clear of the big buttons on narrow views.
        inline Rectangle<int16_t> chat_field_rect()
        {
            const int16_t limit = static_cast<int16_t>(
                big_button_x(0) - 10
            );
            const int16_t right = limit < chat_log_width() - 8
                ? limit
                : static_cast<int16_t>(chat_log_width() - 8);
            return { 92, right, 55, 69 };
        }

        /// X of the whisper/claim buttons, right after the input field.
        inline Point<int16_t> whisper_pos()
        {
            return { static_cast<int16_t>(chat_field_rect().r() + 8), 51 };
        }

        inline Point<int16_t> claim_pos()
        {
            return { static_cast<int16_t>(chat_field_rect().r() + 24), 51 };
        }

        /// X of the chat log slider, relative to the left edge of the log.
        constexpr int16_t chat_slider_x()
        {
            return 550;
        }

        /// Gap between the chat log bottom and the bar top. v83 has no gap:
        /// the input row lives inside the bar.
        constexpr int16_t chat_top_gap()
        {
            return 0;
        }

        // ------------------------------------------------------------------
        // Right side: quickslot display and buttons (right-anchored)
        // ------------------------------------------------------------------

        /// Top-left of base/quickSlot. The panel is one pixel per side wider
        /// than the bar at the top, hence the -9 offset.
        inline Point<int16_t> quickslot_pos()
        {
            return {
                static_cast<int16_t>(Constants::viewwidth() - QUICKSLOT_WIDTH),
                static_cast<int16_t>(BAR_HEIGHT - QUICKSLOT_HEIGHT)
            };
        }

        /// Center of keycap slot `index` (0..7), relative to the panel.
        inline Point<int16_t> quickslot_key_center(int16_t index)
        {
            constexpr int16_t COLUMN_X[4] = { 20, 54, 87, 124 };
            return {
                COLUMN_X[index % 4],
                static_cast<int16_t>(index < 4 ? 24 : 59)
            };
        }

        /// Top-left of the small 28x20 menu shortcut icons (StatKey & co),
        /// laid out as a row left of the quickslot display.
        inline Point<int16_t> icon_button_pos(int16_t index)
        {
            const int16_t row_width = static_cast<int16_t>(6 * 28 + 5 * 2);
            const int16_t x = static_cast<int16_t>(
                quickslot_pos().x() - 8 - row_width + index * 30
            );
            return { x, 5 };
        }

        constexpr int16_t big_button_y()
        {
            return BAR_HEIGHT - 34;
        }

        // ------------------------------------------------------------------
        // Menu popup (UIWindow.img/ContextMenu frame, self-drawn items)
        // ------------------------------------------------------------------

        /// Width of the popup frame (ContextMenu/t).
        constexpr int16_t popup_width()
        {
            return 100;
        }

        /// X of the little pointer tail on ContextMenu/t, for anchoring the
        /// popup to the menu button.
        constexpr int16_t popup_tail_x()
        {
            return 2;
        }

        /// First menu item row, inside ContextMenu/t.
        constexpr int16_t popup_item_top()
        {
            return 10;
        }

        /// Height of one menu item row (ContextMenu/c is 15px).
        constexpr int16_t popup_row_height()
        {
            return 15;
        }

        /// Height of the bottom cap (ContextMenu/s).
        constexpr int16_t popup_bottom_height()
        {
            return 7;
        }

        /// Left/right edge of the item hit rows inside the frame.
        constexpr int16_t popup_item_left()
        {
            return 8;
        }

        constexpr int16_t popup_item_right()
        {
            return 92;
        }
    }
}
