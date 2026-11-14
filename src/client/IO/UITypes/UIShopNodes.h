//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the npc shop window.
//
// Two chrome generations are supported:
//
//   post-Chaos (UIWindow2.img Shop + Shop2, 465x328 window):
//   - every tab/pill shares the window anchor: art = pos - origin, so
//     pos (0,0) reproduces the official layout for all of them
//   - header band y 2..46 (textured, no baked captions); meso box
//     x 352..454, y 64..79 (coin baked x 355..367, the client only
//     draws the right-aligned amount text); dark band y 82..87 and
//     baked column-title traces y 88..91
//   - orange separator y 110..111 across the window, interrupted only
//     by the trough gap x 220..230; sell tabs (42x19 folder art) sit
//     at x 241+43k, y 91..109 enabled / 93..109 disabled, sharing
//     their bottom edge with the separator (enabled pops 2px above)
//   - action pills: BtBuy art (158..222, 45..61), BtSell art
//     (390..454, 45..61) directly above the meso box, BtExit art
//     (158..222, 25..41)
//   - row grid: 5 cells, pitch 42, cell y 115..149 (top rule y 115,
//     faint mid-line y 132, bottom rule y 149), gutter y 150..156,
//     last row bottom edge y 317; window bottom band y 318..327
//   - buy cell: icon slot x 10..44, seam x 45..46, name band x 47..211
//     (165px = the `select` highlight width); sell cell mirrors it at
//     x 241..275 / 278..442
//   - one scrollbar trough between the lists x 227..237, y 28..317;
//     no trough on the right edge
//
//   pre-Chaos (v83, UIWindow.img Shop, 463x339 window):
//   - one background, no backgrnd2/3, no Shop2, row grid first top 124,
//     40px pitch, 35px art, buy x 6..205, sell x 237..436
//   - `select` highlights the name area at column_x + 37
//   - header y 24..83 with baked captions; meso box x 361..451 y 63..80;
//     buy/sell/exit share the header band y 63..81; TabSell art is
//     origin-less and positioned explicitly
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace shop_nodes
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
                ? nl::nx::ui["UIWindow2.img"]["Shop"]
                : nl::nx::ui["UIWindow.img"]["Shop"];
        }

        // Inventory tab strip over the sell list. Both generations keep
        // it in the same subtree as the window chrome (Shop2 hosts the
        // strip of the wider 508px variant with origins shifted by the
        // extra width, which would push our tabs 43px right).
        inline nl::node sell_tabs()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Shop"]["TabSell"]
                : nl::nx::ui["UIWindow.img"]["Shop"]["TabSell"];
        }

        inline int16_t row_top()
        {
            return post_chaos() ? 115 : 124;
        }

        inline int16_t row_step()
        {
            return post_chaos() ? 42 : 40;
        }

        inline int16_t row_height()
        {
            return post_chaos() ? 35 : 35;
        }

        inline int16_t row_width()
        {
            return post_chaos() ? 200 : 199;
        }

        inline int16_t buy_row_x()
        {
            return post_chaos() ? 11 : 6;
        }

        inline int16_t sell_row_x()
        {
            return post_chaos() ? 241 : 237;
        }

        // Buy list item origin (the buy rows are drawn by UIShop itself).
        inline Point<int16_t> buy_itempos(int16_t visible_slot)
        {
            return {
                buy_row_x(),
                static_cast<int16_t>(row_top() + row_step() * visible_slot)
            };
        }

        inline Point<int16_t> buy_item_button_pos(int16_t visible_slot)
        {
            return {
                buy_row_x(),
                static_cast<int16_t>(row_top() + row_step() * visible_slot)
            };
        }

        inline Point<int16_t> sell_item_button_pos(int16_t visible_slot)
        {
            return {
                sell_row_x(),
                static_cast<int16_t>(row_top() + row_step() * visible_slot)
            };
        }

        inline Point<int16_t> recharge_pos(int16_t visible_slot)
        {
            if (post_chaos())
            {
                return { 398, static_cast<int16_t>(125 + 42 * visible_slot) };
            }

            return { 398, static_cast<int16_t>(134 + 40 * visible_slot) };
        }

        // Sell tab art. The art encodes tab k's left edge at
        // pos.x + 241 + 43k and its bottom edge at pos.y + 110, so the
        // shared window anchor (0,0) lands tab k at x 241 + 43k over
        // the sell list with the folder bottom flush on the separator
        // at y 110 (enabled art pops 2px above it, y 91..109).
        inline Point<int16_t> sell_tab_pos(int16_t index)
        {
            if (post_chaos())
            {
                return { 0, 0 };
            }

            return { static_cast<int16_t>(269 + 34 * index), 30 };
        }

        inline Point<int16_t> buy_button_pos()
        {
            return post_chaos() ? Point<int16_t>{ 0, 0 } : Point<int16_t>{ 8, 62 };
        }

        inline Point<int16_t> sell_all_button_pos()
        {
            // Single sell button of the window: it occupies the natural
            // BtSell slot (post-Chaos pill beside the meso box /
            // pre-Chaos header slot).
            // Selling a single item is done by double-clicking a row.
            return post_chaos() ? Point<int16_t>{ 0, 0 } : Point<int16_t>{ 82, 62 };
        }

        inline Point<int16_t> exit_button_pos()
        {
            return post_chaos() ? Point<int16_t>{ 0, 0 } : Point<int16_t>{ 301, 62 };
        }

        // Shift between a row origin and the `select` highlight.
        inline Point<int16_t> selection_shift()
        {
            return post_chaos() ? Point<int16_t>{ 36, 0 } : Point<int16_t>{ 37, 0 };
        }

        inline Point<int16_t> meso_label_pos()
        {
            return post_chaos() ? Point<int16_t>{ 452, 65 } : Point<int16_t>{ 449, 64 };
        }

        // Neither chrome has portrait recesses: the v83 header is a flat
        // caption band and the post-Chaos redesign dropped the npc and
        // charlook renders for full-height lists.
        inline bool has_portraits()
        {
            return false;
        }

        inline Point<int16_t> buy_slider_pos()
        {
            return post_chaos() ? Point<int16_t>{ 227, 28 } : Point<int16_t>{ 212, 124 };
        }

        inline int16_t buy_slider_height()
        {
            return post_chaos() ? 277 : 200;
        }

        inline Point<int16_t> sell_slider_pos()
        {
            return post_chaos() ? Point<int16_t>{ 227, 28 } : Point<int16_t>{ 440, 124 };
        }

        inline int16_t sell_slider_height()
        {
            return post_chaos() ? 277 : 200;
        }

        // The post-Chaos chrome bakes a single trough between the lists;
        // the right edge has none, so the sell slider stays invisible and
        // scrolls through the wheel only.
        inline bool sell_slider_visible()
        {
            return !post_chaos();
        }

        // Hover/click x-ranges of the two lists.
        inline bool cursor_over_buy_list(int16_t xoff)
        {
            return post_chaos() ? (xoff > 10 && xoff < 212) : (xoff > 6 && xoff < 205);
        }

        inline bool cursor_over_sell_list(int16_t xoff)
        {
            return post_chaos() ? (xoff > 240 && xoff < 443) : (xoff > 237 && xoff < 436);
        }

        inline bool scroll_over_buy_list(int16_t xoff)
        {
            return post_chaos() ? (xoff >= 10 && xoff <= 243) : (xoff >= 6 && xoff <= 226);
        }

        inline bool scroll_over_sell_list(int16_t xoff)
        {
            return post_chaos() ? (xoff >= 241 && xoff <= 443) : (xoff >= 237 && xoff <= 456);
        }

        // Vertical band (window-relative) where wheel scrolling is active.
        inline int16_t scroll_y_min()
        {
            return static_cast<int16_t>(row_top() - 6);
        }

        inline int16_t scroll_y_max()
        {
            return static_cast<int16_t>(row_top() + row_step() * 5 + 6);
        }

        // Row content offsets, relative to the row origin. Post-Chaos
        // item icons are bottom-anchored (origin (-3, 30) for a 27x30
        // icon), so (0, 32) drops the art at y 117..147 inside the 35px
        // cell; the name sits in the upper band (y 116..131, faint
        // mid-line y 132) and the meso coin plus price in the lower band
        // (y 133..148). v83 rows are 35px with origin-less icons placed
        // 2px inside the row.
        inline Point<int16_t> icon_offset()
        {
            return post_chaos() ? Point<int16_t>{ 0, 32 } : Point<int16_t>{ 2, 2 };
        }

        inline Point<int16_t> name_offset()
        {
            return post_chaos() ? Point<int16_t>{ 40, 3 } : Point<int16_t>{ 38, 1 };
        }

        inline Point<int16_t> currency_offset()
        {
            return post_chaos() ? Point<int16_t>{ 38, 20 } : Point<int16_t>{ 36, 20 };
        }

        inline Point<int16_t> price_offset()
        {
            return post_chaos() ? Point<int16_t>{ 53, 20 } : Point<int16_t>{ 51, 18 };
        }

        inline Point<int16_t> count_offset()
        {
            return post_chaos() ? Point<int16_t>{ 16, 19 } : Point<int16_t>{ 2, 20 };
        }
    }
}
