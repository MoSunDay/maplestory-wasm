//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the npc storage (trunk) window.
//
// The upstream code reads the trunk from the post-Chaos UIWindow2.img and
// was laid out against its 463x~340 frame. The pre-Chaos (v83) UI.nx has
// no UIWindow2.img: the Trunk subtree lives directly under UIWindow.img
// with a different chrome (no backgrnd2/3, no BtFull/BtSmall/BtSlotEx,
// no disabled slot art, no baked meso node) and a tighter grid:
//
//   - list rows: 5 rows, first row top 84, 40px pitch, 35px art
//     (post-Chaos: 94 / 42 / 36)
//   - left column x 6..205, right column x 237..436
//     (post-Chaos: 8..168 / 177..337)
//   - `en` (199x35) is the v83 empty-icon slot, drawn at the column origin;
//     `select` (162x35) highlights the name area at column_x + 37
//   - two tab strips in the 0..83 header (tab art 27x16, 34px slots at
//     y 59; left strip x 34, right strip x 248); post-Chaos tab art carries
//     its position in the node origin instead
//   - bottom band: parchment strip y 246..289 for the action buttons and
//     a meso row y 292..311 with [coin button][amount box] on both sides
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
        // Generation marker shared with UiAssetVersion.h.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Trunk"]
                : nl::nx::ui["UIWindow.img"]["Trunk"];
        }

        // Row selection highlight. Both generations keep it in the Shop
        // subtree (162x35); only the img root differs.
        inline nl::node selection()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Shop"]["select"]
                : nl::nx::ui["UIWindow.img"]["Shop"]["select"];
        }

        // Empty slot art. Post-Chaos uses Trunk/disabled, v83 ships the
        // `en` icon box (199x35, opaque only over the 36x35 icon slot).
        inline nl::node empty_slot()
        {
            const nl::node trunk = src();
            const nl::node disabled = trunk["disabled"];
            return disabled ? disabled : trunk["en"];
        }

        // Meso coin icon, borrowed from the Shop subtree in both sets.
        inline nl::node meso_icon()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Shop"]["meso"]
                : nl::nx::ui["UIWindow.img"]["Shop"]["meso"];
        }

        // BtFull/BtSmall/BtSlotEx (window mode and slot expansion) only
        // exist post-Chaos.
        inline bool has_mode_buttons()
        {
            return post_chaos();
        }

        inline int16_t row_top()
        {
            return post_chaos() ? 94 : 84;
        }

        inline int16_t row_step()
        {
            return post_chaos() ? 42 : 40;
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

        // X shift between the column origin and the `select` highlight:
        // post-Chaos select spans the whole row from the column origin,
        // v83 select (162x35) starts at the name area (column_x + 37).
        inline int16_t selection_shift()
        {
            return post_chaos() ? 0 : 37;
        }

        // Tab art position. Post-Chaos tab sprites encode their position in
        // the node origin (drawn relative to the window as-is), so the
        // explicit offset stays zero. Pre-Chaos art is origin-less and is
        // arranged in the two header strips.
        inline Point<int16_t> tab_pos(bool right_strip, int16_t index)
        {
            if (post_chaos())
            {
                return {};
            }

            const int16_t strip_x = right_strip ? 248 : 34;
            return { static_cast<int16_t>(strip_x + 3 + 34 * index), 59 };
        }

        // v83 action button positions (bottom parchment strip + meso row).
        // Post-Chaos keeps the historical (unpositioned) placement.
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

        inline Point<int16_t> in_coin_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 8, 294 };
        }

        inline Point<int16_t> out_coin_pos()
        {
            return post_chaos() ? Point<int16_t>{} : Point<int16_t>{ 239, 294 };
        }

        inline Point<int16_t> storage_meso_icon_pos()
        {
            return post_chaos() ? Point<int16_t>{ 142, 317 } : Point<int16_t>{ 46, 295 };
        }

        inline Point<int16_t> player_meso_icon_pos()
        {
            return post_chaos() ? Point<int16_t>{ 314, 317 } : Point<int16_t>{ 277, 295 };
        }

        inline Point<int16_t> storage_meso_label_pos()
        {
            return post_chaos() ? Point<int16_t>{ 173, 319 } : Point<int16_t>{ 193, 297 };
        }

        inline Point<int16_t> player_meso_label_pos()
        {
            return post_chaos() ? Point<int16_t>{ 345, 319 } : Point<int16_t>{ 424, 297 };
        }
    }
}
