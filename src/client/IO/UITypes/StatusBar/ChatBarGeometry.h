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
// Pure geometry for the status-bar chat bar (UIChatbar) and for the
// status-bar hit union.
//
// The chat log hugs the bottom-left corner of the view in both UI
// generations: its left edge is x = 0 and its width is 502px post-Chaos
// and 566px pre-Chaos (the shipped base/chat art width), independent of
// the status bar art. Only the vertical placement differs:
//
//   pre-Chaos (v83): the log sits on top of the 71px StatusBar.img strip,
//                    and the chat input row is part of that strip;
//   post-Chaos:      the log floats 65px above the bar anchor, leaving
//                    room for the floating input row.
//
// Both cases reduce to "`rows` 16px lines on top of a tail band that
// reaches the bottom of the status bar". Draw, resize drag and hit logic
// all derive from that single ChatLayout, so the old 500/502/566 and
// getchattop()-16 discrepancies cannot come back. The resize drag strip
// is a halo centred on the log top that reaches DRAG_HANDLE_ABOVE pixels
// above it, so the log can also GROW, not only shrink.
//
// Nothing here touches OpenGL or NX: tools/verify/chatbar-geometry.cpp
// compiles this module natively.
#pragma once
#include "../../../Constants.h"
#include "../../../Template/Point.h"
#include "../../../Template/Rectangle.h"

#include <cstddef>
#include <cstdint>

namespace jrc
{
    namespace chatbar_geometry
    {
        /// Which status-bar art generation the layout belongs to.
        enum class Generation
        {
            PRECHAOS,
            POSTCHAOS
        };

        // ------------------------------------------------------------------
        // Status-bar strip metrics shared by both layout headers
        // ------------------------------------------------------------------

        constexpr int16_t PRECHAOS_BAR_HEIGHT = 71;
        constexpr int16_t PRECHAOS_BAR_WIDTH = 800;
        constexpr int16_t POSTCHAOS_BAR_HEIGHT = 85;
        constexpr int16_t POSTCHAOS_RIGHT_MARGIN = 288;
        constexpr int16_t POSTCHAOS_LEFT_ANCHOR_X = 512;

        // ------------------------------------------------------------------
        // Chat log metrics
        // ------------------------------------------------------------------

        /// Width of the chat log box: one value for drawing and hit-testing.
        constexpr int16_t CHATBOX_WIDTH = 502;
        /// Width of the pre-Chaos chat row (base/chat art is 566px).
        constexpr int16_t PRECHAOS_CHAT_WIDTH = 566;
        constexpr int16_t CHATROWHEIGHT = 16;
        constexpr int16_t MINCHATROWS = 1;
        constexpr int16_t MAXCHATROWS = 16;

        /// The black log box starts CHATBOX_INSET px below the drag handle
        /// and the separator under its last line adds one more pixel.
        constexpr int16_t CHATBOX_INSET = 2;
        constexpr int16_t CHATBOX_FRAME = CHATBOX_INSET + 1;

        /// Pixels between the top of the log lines and the bottom of the
        /// status bar. Pre-Chaos: the v83 input row lives inside the 71px
        /// strip. Post-Chaos: the floating input row sits in a 65px gap.
        constexpr int16_t PRECHAOS_CHAT_TAIL = PRECHAOS_BAR_HEIGHT;
        constexpr int16_t POSTCHAOS_CHAT_TAIL = 65;

        /// Height of the drag strip below the log top.
        constexpr int16_t DRAG_HANDLE_HEIGHT = 6;
        /// Reach of the drag halo ABOVE the log top, so an upward drag
        /// can grow the log instead of only shrinking it.
        constexpr int16_t DRAG_HANDLE_ABOVE = 6;

        /// Vertical chat-bar layout. `anchor_y` is the bottom edge of the
        /// status bar (viewheight - VIEWYOFFSET), not the element anchor:
        /// pre-Chaos elements anchor at the bar top, post-Chaos ones at the
        /// view bottom, so the bottom edge is the one value both share.
        struct ChatLayout
        {
            int16_t anchor_y = 0;
            int16_t rows = 4;
            int16_t tail = POSTCHAOS_CHAT_TAIL;
            int16_t width = CHATBOX_WIDTH;
        };

        /// Small fixed-capacity rectangle list for the status-bar hit union.
        struct HitTest
        {
            static constexpr std::size_t CAPACITY = 24;

            Rectangle<int16_t> rects[CAPACITY];
            std::size_t count = 0;
        };

        // ------------------------------------------------------------------
        // Status-bar geometry
        // ------------------------------------------------------------------

        constexpr int16_t bar_height(Generation generation)
        {
            return generation == Generation::PRECHAOS
                ? PRECHAOS_BAR_HEIGHT
                : POSTCHAOS_BAR_HEIGHT;
        }

        constexpr int16_t bar_bottom(int16_t view_height)
        {
            return static_cast<int16_t>(view_height - Constants::VIEWYOFFSET);
        }

        constexpr int16_t bar_top(Generation generation, int16_t view_height)
        {
            return static_cast<int16_t>(
                bar_bottom(view_height) - bar_height(generation));
        }

        /// Element anchor of the status bar. The v83 strip hangs from the
        /// bottom-left corner; the post-Chaos bar docks its right control
        /// cluster to the view edge.
        constexpr Point<int16_t> bar_anchor(
            Generation generation, int16_t view_width, int16_t view_height)
        {
            return generation == Generation::PRECHAOS
                ? Point<int16_t>{ 0, bar_top(generation, view_height) }
                : Point<int16_t>{
                    static_cast<int16_t>(view_width - POSTCHAOS_RIGHT_MARGIN),
                    bar_bottom(view_height)
                };
        }

        /// Anchor of the chat bar inside the status bar. The post-Chaos
        /// chat row belongs to the fixed left content column.
        constexpr Point<int16_t> chatbar_anchor(
            Generation generation, int16_t view_height)
        {
            return generation == Generation::PRECHAOS
                ? Point<int16_t>{ 0, bar_top(generation, view_height) }
                : Point<int16_t>{
                    POSTCHAOS_LEFT_ANCHOR_X,
                    bar_bottom(view_height)
                };
        }

        // ------------------------------------------------------------------
        // Chat log geometry
        // ------------------------------------------------------------------

        constexpr int16_t clamp_rows(int16_t rows)
        {
            return rows < MINCHATROWS ? MINCHATROWS
                 : rows > MAXCHATROWS ? MAXCHATROWS
                 : rows;
        }

        // Rows after a resize drag: one row per CHATROWHEIGHT of travel,
        // truncating toward zero so sub-row jitter changes nothing.
        constexpr int16_t rows_after_drag(
            int16_t start_rows, int16_t start_y, int16_t cursor_y)
        {
            const int32_t step =
                (static_cast<int32_t>(cursor_y) - start_y) / CHATROWHEIGHT;
            return clamp_rows(static_cast<int16_t>(start_rows - step));
        }

        constexpr int16_t chat_tail(Generation generation)
        {
            return generation == Generation::PRECHAOS
                ? PRECHAOS_CHAT_TAIL
                : POSTCHAOS_CHAT_TAIL;
        }

        /// Log width per generation: 502px post-Chaos, 566px pre-Chaos.
        constexpr int16_t chat_width(Generation generation)
        {
            return generation == Generation::PRECHAOS
                ? PRECHAOS_CHAT_WIDTH
                : CHATBOX_WIDTH;
        }

        /// Build the layout from the element anchor (UIChatbar::position).
        /// The pre-Chaos anchor is the bar top; the post-Chaos anchor is
        /// already the bar bottom.
        constexpr ChatLayout chat_layout_from_anchor(
            Point<int16_t> anchor, int16_t rows, Generation generation)
        {
            return {
                generation == Generation::PRECHAOS
                    ? static_cast<int16_t>(anchor.y() + PRECHAOS_BAR_HEIGHT)
                    : anchor.y(),
                clamp_rows(rows),
                chat_tail(generation),
                chat_width(generation)
            };
        }

        /// Height of the open chat area from the drag handle to the bar
        /// bottom: the log lines plus the tail band (input row / border).
        constexpr int16_t chatbox_height(const ChatLayout& layout)
        {
            return static_cast<int16_t>(
                clamp_rows(layout.rows) * CHATROWHEIGHT + layout.tail);
        }

        /// Top of the chat log (the current getchattop()).
        constexpr int16_t chat_top(const ChatLayout& layout)
        {
            return static_cast<int16_t>(layout.anchor_y - chatbox_height(layout));
        }

        /// The single hit/draw rectangle of the open chat bar. Drawing
        /// uses this for its x/width and the log body inside it; hit and
        /// drag tests use the whole rectangle.
        constexpr Rectangle<int16_t> chatbox_rect(const ChatLayout& layout)
        {
            return {
                0, layout.width,
                chat_top(layout), layout.anchor_y
            };
        }

        /// Black body of the log inside chatbox_rect(): the 2px handle
        /// inset plus one separator pixel above the bar band.
        constexpr Rectangle<int16_t> log_body_rect(const ChatLayout& layout)
        {
            const int16_t top = static_cast<int16_t>(
                chat_top(layout) + CHATBOX_INSET);
            return {
                0, layout.width,
                top,
                static_cast<int16_t>(
                    top + 1 + clamp_rows(layout.rows) * CHATROWHEIGHT)
            };
        }

        /// Drag halo centred on the log top: DRAG_HANDLE_ABOVE pixels
        /// above it (clamped to the screen) so the log can also grow,
        /// DRAG_HANDLE_HEIGHT pixels below it (clamped to the box).
        constexpr Rectangle<int16_t> drag_handle_rect(const ChatLayout& layout)
        {
            const Rectangle<int16_t> box = chatbox_rect(layout);
            const int16_t top = static_cast<int16_t>(
                box.t() - DRAG_HANDLE_ABOVE < 0 ? 0 : box.t() - DRAG_HANDLE_ABOVE);
            const int16_t bottom = static_cast<int16_t>(
                box.t() + DRAG_HANDLE_HEIGHT < box.b()
                    ? box.t() + DRAG_HANDLE_HEIGHT
                    : box.b());
            return { box.l(), box.r(), top, bottom };
        }

        // ------------------------------------------------------------------
        // Status-bar hit union
        // ------------------------------------------------------------------

        constexpr void add_hit_rect(HitTest& test, Rectangle<int16_t> rect)
        {
            if (test.count < HitTest::CAPACITY)
            {
                test.rects[test.count++] = rect;
            }
        }

        constexpr bool hit_any(const HitTest& test, Point<int16_t> point)
        {
            for (std::size_t i = 0; i < test.count; ++i)
            {
                if (test.rects[i].contains(point))
                {
                    return true;
                }
            }
            return false;
        }

        /// Chat-bar layout derived from the view height alone (the log is
        /// pinned to x = 0 in both generations).
        ChatLayout chat_layout_for_screen(
            Generation generation, int16_t view_height, int16_t rows);

        /// A chat bar only reacts while open, and only inside its log.
        bool chatbox_hit(
            bool open, const ChatLayout& layout, Point<int16_t> point);

        /// True when the point lands on one of the supplied status-bar
        /// controls or on the open chat log (`chatbox`). Blank bar
        /// background misses.
        bool statusbar_hit_test(
            const HitTest& controls, bool chat_open,
            Rectangle<int16_t> chatbox, Point<int16_t> point);
    }
}
