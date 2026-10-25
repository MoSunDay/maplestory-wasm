//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the npc storage (trunk) window.
//
// Two trunk generations ship in UI.nx:
//
// post-Chaos (UIWindow2.img/Trunk) is the 347x414 grid window, decoded
// from the shipped art:
//   - backgrnd (347x414) is the shadow layer, backgrnd2 (333x400 at
//     (-7,-7)) the beige plate and backgrnd3 (331x392 at (-8,-11)) the
//     socket layer with the grid carved in as transparent seams;
//   - two 4x8 socket panels of 33x33 cells (36px column pitch, 35px row
//     pitch), first cells at (-4,78) and (169,78), separated by the
//     x136..167 divider gutter; `Trunk/disabled` (33x33) is the empty
//     cell art;
//   - a transparent tab band y51..73 above the right panel and a
//     parchment strip y365..380 under the grid (y392..413 is shadow);
//   - button/tab art bakes its authored spot into the sprite origins and
//     MapleButton/TwoSpriteButton anchor the final rectangle at
//     position - origin, so the helpers only carry deltas off those
//     authored spots.
//
// legacy (v83, UIWindow.img/Trunk) is the 463x318 list frame (no
// backgrnd2/3, no BtFull/BtSmall/BtSlotEx, no disabled slot art, no
// baked meso node) and is preferred whenever it is present:
//   - list rows: 4 visible rows, first row top 84, 40px pitch, 35px art;
//     left column x 6..205, right column x 237..436
//   - `en` (199x35) is the v83 empty-icon slot, drawn at the column
//     origin; `select` (162x35) highlights the name area at column_x+37
//   - two tab strips in the 0..83 header (tab art 27x16, 34px slots at
//     y 59; left strip x 34, right strip x 248); parchment strip y246..289
//     for the action buttons, meso row y292..311
//
// All lookups are null safe and every generation branch keeps the
// post-Chaos coordinates when the post-Chaos assets are loaded.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace storage_nodes
    {
        // Post-Chaos grid geometry, measured from the shipped
        // UIWindow2.img/Trunk pixels (transparent seams of backgrnd3).
        // Exposed as constants so the standalone verifier can decode the
        // art and check the layout against them. The tab band is
        // y51..73 / x147..322 and the parchment meso strip y365..380; the
        // tab sprites measure 30x17 (disabled) / 30x19 (enabled), so a
        // 30px pitch lays them out edge to edge.
        constexpr int16_t GRID_COLS = 4;
        constexpr int16_t GRID_ROWS = 8;
        constexpr int16_t GRID_COL_PITCH = 36;
        constexpr int16_t GRID_ROW_PITCH = 35;
        constexpr int16_t GRID_CELL_SIZE = 33;
        constexpr Point<int16_t> GRID_LEFT_ORIGIN = { -4, 78 };
        constexpr Point<int16_t> GRID_RIGHT_ORIGIN = { 169, 78 };
        constexpr Point<int16_t> GRID_TAB_ORIGIN = { 167, 52 };
        constexpr int16_t GRID_TAB_PITCH = 30;
        constexpr Point<int16_t> GRID_IN_COIN_POS = { 184, 364 };
        constexpr Point<int16_t> GRID_OUT_COIN_POS = { 11, 364 };
        constexpr Point<int16_t> GRID_STORAGE_MESO_ICON_POS = { 52, 366 };
        constexpr Point<int16_t> GRID_PLAYER_MESO_ICON_POS = { 230, 366 };
        constexpr Point<int16_t> GRID_STORAGE_MESO_LABEL_POS = { 68, 367 };
        constexpr Point<int16_t> GRID_PLAYER_MESO_LABEL_POS = { 246, 367 };

        // Trunk art generation: the dump marker reports post-Chaos, but
        // the shipped UI.nx still carries the legacy list-style Trunk
        // window (UIWindow.img/Trunk with the `en` row art, 463x318),
        // which wins whenever its art is present.
        inline bool legacy()
        {
            const nl::node row_art = nl::nx::ui["UIWindow.img"]["Trunk"]["en"];
            return static_cast<bool>(row_art);
        }

        inline bool post_chaos()
        {
            return !legacy();
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Trunk"]
                : nl::nx::ui["UIWindow.img"]["Trunk"];
        }

        // Row selection highlight (162x35) for the legacy list; the
        // post-Chaos grid highlights the cell with a ColorBox instead.
        inline nl::node selection()
        {
            if (post_chaos())
            {
                return nl::nx::ui["UIWindow2.img"]["Shop"]["select"];
            }

            const nl::node trunk_select = nl::nx::ui["UIWindow.img"]["Trunk"]["select"];
            return trunk_select ? trunk_select : nl::nx::ui["UIWindow.img"]["Shop"]["select"];
        }

        // Empty slot art: Trunk/disabled (33x33, cell corner aligned)
        // post-Chaos, the v83 `en` icon box (199x35) otherwise.
        inline nl::node empty_slot()
        {
            const nl::node trunk = src();
            const nl::node disabled = trunk["disabled"];
            return disabled ? disabled : trunk["en"];
        }

        // Meso coin icon (12x12, origin-less). The legacy trunk window has
        // no coin art of its own and a modern UIWindow.img carries no Shop
        // subtree, so borrow the post-Chaos coin first and fall back to
        // the v83 Shop copy.
        inline nl::node meso_icon()
        {
            if (post_chaos())
            {
                return nl::nx::ui["UIWindow2.img"]["Shop"]["meso"];
            }

            const nl::node coin = nl::nx::ui["UIWindow2.img"]["Shop"]["meso"];
            return coin ? coin : nl::nx::ui["UIWindow.img"]["Shop"]["meso"];
        }

        // BtFull/BtSmall/BtSlotEx (mode/slot expansion) only exist post-Chaos.
        inline bool has_mode_buttons()
        {
            return post_chaos();
        }

        inline int16_t row_top()
        {
            return post_chaos() ? 94 : 84;
        }

        // Vertical distance between two sockets/rows: grid row pitch 35,
        // legacy list pitch 40.
        inline int16_t row_step()
        {
            return post_chaos() ? GRID_ROW_PITCH : 40;
        }

        inline int16_t row_height()
        {
            return post_chaos() ? 36 : 35;
        }

        inline int16_t left_row_x()
        {
            return post_chaos() ? 8 : 6;
        }

        inline int16_t right_row_x()
        {
            return post_chaos() ? 177 : 237;
        }

        inline int16_t row_width()
        {
            return post_chaos() ? 160 : 199;
        }

        inline int16_t left_icon_x()
        {
            return post_chaos() ? 17 : 10;
        }

        inline int16_t right_icon_x()
        {
            return post_chaos() ? 186 : 241;
        }

        inline int16_t icon_y()
        {
            return post_chaos() ? 99 : 86;
        }

        // Visible rows per panel: eight grid rows vs four list rows.
        inline int16_t rows_visible()
        {
            return post_chaos() ? GRID_ROWS : 4;
        }

        // Socket columns per panel (legacy list = single column) and the
        // visible cells per panel.
        inline int16_t grid_cols()
        {
            return post_chaos() ? GRID_COLS : 1;
        }

        inline int16_t cells_visible()
        {
            return static_cast<int16_t>(rows_visible() * grid_cols());
        }

        // Grid column pitch (36); the legacy single column has none.
        inline int16_t col_pitch()
        {
            return post_chaos() ? GRID_COL_PITCH : 0;
        }

        // Window position of a panel's first cell: the grid panels start
        // at their measured origins, the legacy list at the column origin.
        inline Point<int16_t> panel_origin(bool inventory)
        {
            if (post_chaos())
            {
                return inventory ? GRID_RIGHT_ORIGIN : GRID_LEFT_ORIGIN;
            }

            return Point<int16_t>(
                inventory ? right_row_x() : left_row_x(),
                row_top());
        }

        // Window position of visible cell `i` (row-major over the
        // panel): grid cells walk 4 columns x 8 rows from the panel
        // origin, legacy cells walk the single list column.
        inline Point<int16_t> cell_pos(bool inventory, int16_t i)
        {
            if (post_chaos())
            {
                const Point<int16_t> origin = panel_origin(inventory);
                return Point<int16_t>(
                    static_cast<int16_t>(origin.x() + (i % grid_cols()) * col_pitch()),
                    static_cast<int16_t>(origin.y() + (i / grid_cols()) * row_step()));
            }

            return Point<int16_t>(
                inventory ? right_row_x() : left_row_x(),
                static_cast<int16_t>(row_top() + i * row_step()));
        }

        // Legacy list icon anchor (inside the row backdrop); grid icons
        // anchor on the cell corner itself (see UIStorage.cpp).
        inline Point<int16_t> list_icon_pos(bool inventory, int16_t i)
        {
            return Point<int16_t>(
                inventory ? right_icon_x() : left_icon_x(),
                static_cast<int16_t>(icon_y() + i * row_step()));
        }

        // Cells per scroll notch: one grid row (4 sockets) or one list row.
        inline int16_t grid_scroll_step()
        {
            return post_chaos() ? grid_cols() : 1;
        }

        // 0-based visible cell under a window-relative cursor position, or
        // -1 outside the panel. The grid test keeps the item-inventory
        // [1, span-1] margin so the border is not clickable; the legacy
        // test keeps the historical slot_by_position semantics (x inside
        // the column width, y inside a row's 35px art).
        inline int16_t cell_at(bool inventory, Point<int16_t> cur)
        {
            if (post_chaos())
            {
                const Point<int16_t> origin = panel_origin(inventory);
                const int16_t xoff = static_cast<int16_t>(cur.x() - origin.x());
                const int16_t yoff = static_cast<int16_t>(cur.y() - origin.y());
                const int16_t xspan = static_cast<int16_t>(grid_cols() * col_pitch());
                const int16_t yspan = static_cast<int16_t>(GRID_ROWS * row_step());
                if (xoff < 1 || xoff > xspan - 1 || yoff < 1 || yoff > yspan - 1)
                    return -1;

                return static_cast<int16_t>(
                    xoff / col_pitch() + grid_cols() * (yoff / row_step()));
            }

            const int16_t column_x = inventory ? right_row_x() : left_row_x();
            if (cur.x() < column_x || cur.x() > column_x + row_width())
                return -1;

            const int16_t yoff = static_cast<int16_t>(cur.y() - row_top());
            if (yoff < 0)
                return -1;

            const int16_t slot = static_cast<int16_t>(yoff / row_step());
            if (slot >= rows_visible())
                return -1;

            if (yoff - slot * row_step() >= row_height())
                return -1;

            return slot;
        }

        // Offset from the cell to the empty-slot art anchor (none needed).
        inline Point<int16_t> empty_slot_shift()
        {
            return Point<int16_t>();
        }

        // X shift between the column origin and the `select` highlight:
        // post-Chaos select spans the whole row from the column origin,
        // v83 select (162x35) starts at the name area (column_x + 37).
        inline int16_t selection_shift()
        {
            return post_chaos() ? 0 : 37;
        }

        // Tab art position. Post-Chaos tab sprites bake an authored spot
        // of (183 + 31*i, 78) into their origins, which overflows the
        // y51..73 tab band onto the first socket row, so the strip is
        // re-laid out 30px apart from GRID_TAB_ORIGIN; buttons anchor at
        // position - origin, hence the delta off the authored spot.
        // Pre-Chaos art is origin-less and is arranged in the two header
        // strips.
        inline Point<int16_t> tab_pos(bool right_strip, int16_t index)
        {
            if (post_chaos())
            {
                constexpr int16_t authored_x = 183;
                constexpr int16_t authored_pitch = 31;
                constexpr int16_t authored_y = 78;
                return Point<int16_t>(
                    static_cast<int16_t>(GRID_TAB_ORIGIN.x() + GRID_TAB_PITCH * index - authored_x - authored_pitch * index),
                    static_cast<int16_t>(GRID_TAB_ORIGIN.y() - authored_y));
            }

            const int16_t strip_x = right_strip ? 248 : 34;
            return { static_cast<int16_t>(strip_x + 3 + 34 * index), 59 };
        }

        // Action button positions. Post-Chaos button art carries its
        // authored spot in the sprite origin (BtExit (94,17), BtGet
        // (94,36), BtSort (94,55), BtPut (255,17)), so those buttons
        // position themselves and the helpers stay zero. v83 art is
        // origin-less and is placed explicitly on the parchment strip.
        inline Point<int16_t> get_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 28, 247 };
        }

        inline Point<int16_t> put_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 110, 247 };
        }

        inline Point<int16_t> sort_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 28, 272 };
        }

        inline Point<int16_t> exit_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 110, 271 };
        }

        // Coin buttons (33x16). Post-Chaos origins bake in the authored
        // bottom-edge spot ((184,387)/(11,387)); the delta moves them up
        // onto the parchment meso strip. v83 is placed on its meso row.
        inline Point<int16_t> in_coin_pos()
        {
            if (post_chaos())
            {
                constexpr Point<int16_t> authored = { 184, 387 };
                return GRID_IN_COIN_POS - authored;
            }

            return { 8, 294 };
        }

        inline Point<int16_t> out_coin_pos()
        {
            if (post_chaos())
            {
                constexpr Point<int16_t> authored = { 11, 387 };
                return GRID_OUT_COIN_POS - authored;
            }

            return { 239, 294 };
        }

        // Meso readouts on the parchment strip (post-Chaos plates at
        // x33..144 / x206..299 in bg3; v83 meso row y292..311).
        inline Point<int16_t> storage_meso_icon_pos()
        {
            return post_chaos() ? GRID_STORAGE_MESO_ICON_POS : Point<int16_t>{ 46, 295 };
        }

        inline Point<int16_t> player_meso_icon_pos()
        {
            return post_chaos() ? GRID_PLAYER_MESO_ICON_POS : Point<int16_t>{ 277, 295 };
        }

        inline Point<int16_t> storage_meso_label_pos()
        {
            return post_chaos() ? GRID_STORAGE_MESO_LABEL_POS : Point<int16_t>{ 193, 297 };
        }

        inline Point<int16_t> player_meso_label_pos()
        {
            return post_chaos() ? GRID_PLAYER_MESO_LABEL_POS : Point<int16_t>{ 424, 297 };
        }
    }
}
