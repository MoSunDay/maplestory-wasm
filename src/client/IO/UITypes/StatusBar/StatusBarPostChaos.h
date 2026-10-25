//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the License as published by the Free Software      //
// Foundation.  See the NOTICE file for details.                            //
//////////////////////////////////////////////////////////////////////////////
// Free-function coordinate vocabulary for the post-Chaos status bar
// (StatusBar2.img/mainBar, 1366x85) plus the generation dispatchers shared
// by the quickslot dock, which exists in both asset generations.
//
// Layout metrics below were measured from the actual UI.nx textures:
//   mainBar/backgrnd          1366x85  origin (512,84)
//   mainBar/quickSlot/
//     quickSlot               145x93   origin (-143,143)
//   mainBar/BtSystem states   55x35    origin (-230,36)
//
// The element anchor keeps the bar's right control cluster (quickslot,
// BtSystem, icon row) docked to the right edge of the live view: the
// quickslot bitmap's right edge lands exactly on P.x + 288, so the anchor
// x is viewwidth() - 288. At the default 800px view this reproduces the
// original UIStatusbar::POSITION.x of 512.
#pragma once
#include "../../../Constants.h"
#include "StatusBarPreChaos.h"
#include "QuickSlotModel.h"

namespace jrc
{
    namespace statusbar
    {
        // ------------------------------------------------------------------
        // Post-Chaos bar geometry
        // ------------------------------------------------------------------

        /// Distance from the element anchor to the right edge of the bar's
        /// control cluster (quickslot bitmap: 143px offset + 145px wide).
        constexpr int16_t CHAOS_RIGHT_MARGIN = 288;

        /// Size of the post-Chaos bottom strip bitmap (backgrnd 1366x85).
        constexpr int16_t CHAOS_BAR_WIDTH = 1366;
        constexpr int16_t CHAOS_BAR_HEIGHT = 85;

        /// Top edge of the strip inside the live view.
        inline int16_t postchaos_bar_top()
        {
            return static_cast<int16_t>(
                Constants::viewheight() - Constants::VIEWYOFFSET
                - CHAOS_BAR_HEIGHT);
        }

        /// Element anchor of the post-Chaos bar. The textures carry their
        /// own nx origins relative to this anchor; the y places the bar
        /// bottom one VIEWYOFFSET above the view bottom, like the v83 bar.
        inline Point<int16_t> postchaos_bar_position()
        {
            return {
                static_cast<int16_t>(
                    Constants::viewwidth() - CHAOS_RIGHT_MARGIN),
                static_cast<int16_t>(
                    Constants::viewheight() - Constants::VIEWYOFFSET)
            };
        }

        /// Top edge of the shortcut icon row relative to the element anchor
        /// (MapleButton uses the NORMAL origin, y = 57).
        constexpr int16_t CHAOS_SHORTCUT_ICON_TOP = -57;

        /// Top-left of mainBar/quickSlot/quickSlot relative to the element
        /// anchor. The nx origin (-143,143) docks the panel above the bar;
        /// it is lifted two pixels further so the panel's 93px body clears
        /// the 21px shortcut icon row (panel bottom below icon top).
        constexpr int16_t CHAOS_QUICKSLOT_OFFSET_Y =
            CHAOS_SHORTCUT_ICON_TOP - quickslot::CHAOS_LAYOUT.dimension.y() - 2;
        constexpr Point<int16_t> CHAOS_QUICKSLOT_OFFSET = {
            143, CHAOS_QUICKSLOT_OFFSET_Y
        };

        static_assert(
            CHAOS_QUICKSLOT_OFFSET_Y + quickslot::CHAOS_LAYOUT.dimension.y()
                <= CHAOS_SHORTCUT_ICON_TOP,
            "quickslot panel must not overlap the shortcut icon row"
        );

        // ------------------------------------------------------------------
        // Left cluster (level block, gauges, chat row)
        // ------------------------------------------------------------------

        /// Anchor of the left content column. Every mainBar texture that
        /// positions with a POSITIVE origin x (level block, gauge frame,
        /// chat row) is calibrated for an 800px view with the bar's left
        /// edge at x = 0, i.e. an element x of 512. Anchoring that column
        /// at a fixed 512 keeps the 800px look at every view width while
        /// the right cluster (negative origins) docks to the view's right
        /// edge and the backgrnd covers the full width - at the default
        /// 800px view both anchors coincide.
        inline Point<int16_t> postchaos_left_anchor()
        {
            return {
                512,
                static_cast<int16_t>(
                    Constants::viewheight() - Constants::VIEWYOFFSET)
            };
        }

        // ------------------------------------------------------------------
        // Generation dispatchers (quickslot dock)
        // ------------------------------------------------------------------

        /// Quickslot layout of the loaded asset generation.
        inline const quickslot::Layout& quickslot_layout()
        {
            return prechaos()
                ? quickslot::V83_LAYOUT
                : quickslot::CHAOS_LAYOUT;
        }

        /// Top-left of the quickslot panel for the element at `position`.
        /// v83 docks its own bitmap at the view's right edge; post-Chaos
        /// draws the mainBar bitmap straight at the anchor.
        inline Point<int16_t> quickslot_panel_pos(Point<int16_t> position)
        {
            return prechaos()
                ? position + quickslot_pos()
                : position + CHAOS_QUICKSLOT_OFFSET;
        }

        /// Hit-test bounds of the quickslot panel for the element at
        /// `position`, sized after the active generation's bitmap.
        inline Rectangle<int16_t> quickslot_dock_range(Point<int16_t> position)
        {
            const Point<int16_t> pos = quickslot_panel_pos(position);
            return {
                pos,
                pos + quickslot_layout().dimension
            };
        }
    }
}
