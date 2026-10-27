//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the License as published by the Free Software      //
// Foundation.  See the NOTICE file for details.                            //
//////////////////////////////////////////////////////////////////////////////
// Free-function coordinate vocabulary for the post-Chaos status bar
// (StatusBar2.img/mainBar, 1366x85).
//
// Layout metrics below were measured from the actual UI.nx textures:
//   mainBar/backgrnd          1366x85  origin (512,84)
//   mainBar/BtSystem states   55x35    origin (-230,36)
//
// The element anchor keeps the bar's right control cluster (BtSystem,
// icon row) docked to the right edge of the live view: the anchor x is
// viewwidth() - 288, which at the default 800px view reproduces the
// original UIStatusbar::POSITION.x of 512.
#pragma once
#include "../../../Constants.h"
#include "StatusBarPreChaos.h"

namespace jrc
{
    namespace statusbar
    {
        // ------------------------------------------------------------------
        // Post-Chaos bar geometry
        // ------------------------------------------------------------------

        /// Distance from the element anchor to the right edge of the bar's
        /// control cluster (BtSystem, icon row). Kept at the historic 288px
        /// (143px offset + 145px width of the removed quickslot bitmap) so
        /// every retained right-cluster control stays at its screen spot.
        constexpr int16_t CHAOS_RIGHT_MARGIN = 288;

        /// Height of the post-Chaos bottom strip bitmap (backgrnd
        /// 1366x85).
        constexpr int16_t CHAOS_BAR_HEIGHT = 85;

        /// Width of the art-free left part of the 1366px strip bitmap. The
        /// shipped backgrnd bakes a 33px-pitch quickslot-cell grid into its
        /// right end (first border at x=880) that overlaps the SHOP/MTS/
        /// Menu/System cluster, so only this flat part is drawn.
        constexpr int16_t CHAOS_BACKGRND_FLAT_WIDTH = 878;

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
    }
}
