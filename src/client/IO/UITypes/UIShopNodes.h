//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the npc shop window.
//
// The upstream code reads the shop from the post-Chaos UIWindow2.img
// (Shop + Shop2) and is laid out against its frame. The pre-Chaos (v83)
// UI.nx ships a single Shop subtree under UIWindow.img with a different
// chrome:
//
//   - one background (463x339), no backgrnd2/3, no Shop2, no
//     ShopSpecial/impossible art
//   - row grid: 5 rows, first row top 124, 40px pitch, 35px art
//     (post-Chaos: 116 / 42 / 36); left column x 6..205 (buy list),
//     right column x 237..436 (sell list)
//   - `select` (162x35) highlights the name area starting at
//     column_x + 37 (post-Chaos draws it from the row origin)
//   - header y 24..83 with baked list captions; the meso box sits at
//     x 361..451, y 63..80 (post-Chaos label: (450, 62)); no portrait
//     areas for the npc/charlook renders
//   - TabSell/TabBuy art is origin-less (27x16) and must be positioned
//     explicitly; post-Chaos art encodes its position in the node origin
//   - no bottom button strip: the buy/sell/exit controls share the
//     header band (y 63..81) with the meso display
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

        // Inventory tab strip over the sell list. Post-Chaos keeps it in
        // the Shop2 subtree, v83 in Shop itself (7 states; the client uses
        // the first 5).
        inline nl::node sell_tabs()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Shop2"]["TabSell"]
                : nl::nx::ui["UIWindow.img"]["Shop"]["TabSell"];
        }

        inline int16_t row_top()
        {
            return post_chaos() ? 116 : 124;
        }

        inline int16_t row_step()
        {
            return post_chaos() ? 42 : 40;
        }

        inline int16_t row_height()
        {
            return post_chaos() ? 36 : 35;
        }

        inline int16_t row_width()
        {
            return post_chaos() ? 200 : 199;
        }

        inline int16_t buy_row_x()
        {
            return post_chaos() ? 8 : 6;
        }

        inline int16_t sell_row_x()
        {
            return post_chaos() ? 242 : 237;
        }

        // Buy list item origin (the buy rows are drawn by UIShop itself).
        inline Point<int16_t> buy_itempos(int16_t visible_slot)
        {
            return {
                buy_row_x(),
                static_cast<int16_t>(row_top() + row_step() * visible_slot)
            };
        }

        // The sell rows are drawn by SellState with a hardcoded
        // (243, 116 + 42 * slot) origin; shifting the parent position by
        // this delta realigns them with the v83 grid.
        inline Point<int16_t> sell_parent_shift()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 0, 8 };
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
                return { 398, static_cast<int16_t>(133 + 42 * visible_slot) };
            }

            return { 398, static_cast<int16_t>(134 + 40 * visible_slot) };
        }

        // Sell tab art. Post-Chaos art positions itself through the node
        // origin combined with the historical (-43, -9) shift.
        inline Point<int16_t> sell_tab_pos(int16_t index)
        {
            if (post_chaos())
            {
                return { -43, -9 };
            }

            return { static_cast<int16_t>(269 + 34 * index), 30 };
        }

        inline Point<int16_t> buy_button_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 8, 62 };
        }

        inline Point<int16_t> sell_button_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 82, 62 };
        }

        inline Point<int16_t> sell_all_button_pos()
        {
            return post_chaos() ? Point<int16_t>{ 0, -20 } : Point<int16_t>{ 156, 62 };
        }

        inline Point<int16_t> exit_button_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 301, 62 };
        }

        // Shift between a row origin and the `select` highlight.
        inline Point<int16_t> selection_shift()
        {
            return post_chaos() ? Point<int16_t>{ 35, -1 } : Point<int16_t>{ 37, 0 };
        }

        inline Point<int16_t> meso_label_pos()
        {
            return post_chaos() ? Point<int16_t>{ 450, 62 } : Point<int16_t>{ 449, 64 };
        }

        // The v83 header is a flat caption band without portrait recesses.
        inline bool has_portraits()
        {
            return post_chaos();
        }

        inline Point<int16_t> buy_slider_pos()
        {
            return post_chaos() ? Point<int16_t>{ 214, 115 } : Point<int16_t>{ 212, 124 };
        }

        inline int16_t buy_slider_height()
        {
            return post_chaos() ? 193 : 200;
        }

        inline Point<int16_t> sell_slider_pos()
        {
            return post_chaos() ? Point<int16_t>{ 445, 115 } : Point<int16_t>{ 440, 124 };
        }

        inline int16_t sell_slider_height()
        {
            return post_chaos() ? 193 : 200;
        }

        // Hover/click x-ranges of the two lists.
        inline bool cursor_over_buy_list(int16_t xoff)
        {
            return post_chaos() ? (xoff > 7 && xoff < 209) : (xoff > 6 && xoff < 205);
        }

        inline bool cursor_over_sell_list(int16_t xoff)
        {
            return post_chaos() ? (xoff > 241 && xoff < 443) : (xoff > 237 && xoff < 436);
        }

        inline bool scroll_over_buy_list(int16_t xoff)
        {
            return post_chaos() ? (xoff >= 7 && xoff <= 223) : (xoff >= 6 && xoff <= 226);
        }

        inline bool scroll_over_sell_list(int16_t xoff)
        {
            return post_chaos() ? (xoff >= 241 && xoff <= 454) : (xoff >= 237 && xoff <= 456);
        }

        // Item icon y-offset inside a row. Post-Chaos item icons are
        // bottom-anchored (origin y 32), v83 icons are origin-less and are
        // placed 2px inside the 35px row.
        inline int16_t icon_offset_y()
        {
            return post_chaos() ? 32 : 2;
        }
    }
}
