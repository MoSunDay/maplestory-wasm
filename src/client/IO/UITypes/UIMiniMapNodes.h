//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the minimap.
//
// The upstream code reads the minimap chrome from the post-Chaos
// UIWindow2.img and was laid out against its art (64px wide corner pieces,
// 17px title band, size/npc-list buttons on a -6px overhang). The pre-Chaos
// (v83) UI.nx has no UIWindow2.img; MiniMap lives under UIWindow.img with a
// different chrome:
//
//   - BtMap (36x12) is the ONLY chrome button: no BtMin/BtMax/BtSmall/
//     BtBig/BtNpc, so the size buttons and the npc list cannot be created
//   - Min bar: w 8x20 / c 1x20 stretch / e 4x20
//   - MinMap (normal window): nine-slice with 6px rails, 29px title band,
//     14px bottom band
//   - MaxMap (big window): same nine-slice with a 72px title band and a
//     15px bottom band (map name + map mark live in the tall band)
//   - no ListNpc subtree and no iconNpc marker
//
// All lookups are null safe and every generation branch keeps the
// post-Chaos coordinates when the post-Chaos assets are loaded.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace minimap_nodes
    {
        // Generation marker shared with UiAssetVersion.h.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["MiniMap"]
                : nl::nx::ui["UIWindow.img"]["MiniMap"];
        }

        inline nl::node npclist_src()
        {
            return src()["ListNpc"];
        }

        // The post-Chaos helper override; absent in v83.
        inline nl::node simple_helper_src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["MiniMapSimpleMode"]["DefaultHelper"]
                : nl::node();
        }

        // v83 only ships BtMap; the size buttons and the npc list toggle
        // are post-Chaos chrome.
        inline bool has_size_buttons()
        {
            return static_cast<bool>(src()["BtMin"]);
        }

        inline bool has_npc_button()
        {
            return static_cast<bool>(src()["BtNpc"]);
        }

        inline bool has_npclist()
        {
            return static_cast<bool>(npclist_src()["c"]);
        }

        // ---- v83 frame art (measured from the bitmaps) -------------------

        constexpr int16_t SIDE_W = 6;
        constexpr int16_t NORMAL_TOP_H = 29;
        constexpr int16_t NORMAL_BOTTOM_H = 14;
        constexpr int16_t MAX_TOP_H = 72;
        constexpr int16_t MAX_BOTTOM_H = 15;
        constexpr int16_t MIN_W = 8;
        constexpr int16_t MIN_E = 4;
        constexpr int16_t MIN_H = 20;

        // The big window only differs from the normal one by its taller
        // title band; markers keep the post-Chaos +40 shift.
        inline int16_t max_adjust()
        {
            return post_chaos() ? 40 : MAX_TOP_H - NORMAL_TOP_H;
        }

        // ---- v83 positions ----------------------------------------------

        // Width of the collapsed bar: [w][pad 7 + text + gap 6 + button][e].
        inline int16_t min_bar_width(int16_t text_width, int16_t map_button_width)
        {
            const int16_t content = 7 + text_width + 6 + map_button_width;
            return MIN_W + std::max<int16_t>(MIN_W + MIN_E, content) + MIN_E;
        }

        // BtMap sits at the right edge of whichever bar/band hosts it.
        inline Point<int16_t> map_button_pos(int16_t window_width, int16_t band_height, int16_t button_width)
        {
            return Point<int16_t>(
                window_width - MIN_E - button_width,
                std::max<int16_t>(0, (band_height - 12) / 2)
            );
        }

        // Map mark and the two name lines inside the tall v83 title band.
        inline Point<int16_t> max_mark_pos()
        {
            return Point<int16_t>(13, 18);
        }

        inline Point<int16_t> max_region_pos()
        {
            return Point<int16_t>(48, 18);
        }

        inline Point<int16_t> max_town_pos()
        {
            return Point<int16_t>(48, 34);
        }

        // Collapsed-bar text anchor.
        inline Point<int16_t> min_text_pos()
        {
            return Point<int16_t>(MIN_W - 1, 4);
        }
    }
}
