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
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
// Node paths and layout geometry for the item inventory window, per UI.nx
// generation.
//
// post-Chaos (upstream) reads UIWindow2.img/Item: a 172x335 window whose
// sockets (backgrnd2) and tab strip / meso row (backgrnd3) live on their
// own layers, with origin-bearing button art (BtCoin/BtGather/BtSort/
// BtFull plus the post-Chaos-only BtPoint0/BtPot3/BtUpgrade3/BtAppraise3/
// BtBits3) and a 35px socket row pitch.
//
// pre-Chaos (v83) has no UIWindow2.img; the inventory lives under
// UIWindow.img/Item as a single 175x288 backgrnd with everything baked
// in: origin-less tab sprites laid out 32px apart, centre-anchored (or
// plain) button frames placed on their sockets, a 34px socket row pitch
// and none of the extra buttons.
//
// These free functions pick the path / geometry matching the loaded
// asset set (marker shared with UiAssetVersion.h) so the window code can
// stay linear. Every node lookup is null safe.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Character/Inventory/InventoryType.h"
#include "../../Template/Point.h"
#include "../../Template/Range.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace iteminv_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos (v83) does not.
        inline bool post_chaos()
        {
            const nl::node marker =
                nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        // Window root: UIWindow2.img/Item post-Chaos, UIWindow.img/Item in
        // v83.
        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Item"]
                : nl::nx::ui["UIWindow.img"]["Item"];
        }

        // Post-Chaos paints sockets and tab strip / meso row on the
        // backgrnd2/backgrnd3 layers; v83 bakes everything into the single
        // backgrnd and the extra layers do not exist.
        inline bool has_layered_background()
        {
            return post_chaos();
        }

        // Default drag anchor; the v83 window is 3px wider.
        inline Point<int16_t> drag_position()
        {
            return post_chaos()
                ? Point<int16_t>(172, 20)
                : Point<int16_t>(175, 20);
        }

        inline Point<int16_t> dimension()
        {
            return post_chaos()
                ? Point<int16_t>(172, 335)
                : Point<int16_t>(175, 289);
        }

        // Tab strip. Post-Chaos tab sprites carry origins, so the strip
        // only needs the upstream nudge (the third sprite is 1px wider).
        // v83 sprites carry no origin, so the five 27px tabs are laid out
        // explicitly, 32px apart, on the band under the title bar.
        inline Point<int16_t> tab_button_pos(int16_t index)
        {
            if (post_chaos())
                return Point<int16_t>(index == 2 ? 0 : -1, -4);
            return Point<int16_t>(9 + 32 * index, 24);
        }

        // The meso point wallet and the item pot / upgrade / magnify /
        // bit case panels only exist post-Chaos; v83 simply omits them.
        inline bool has_extra_buttons()
        {
            return post_chaos();
        }

        // Post-Chaos button art carries origins, so an empty position is
        // enough. v83 frames are centre-anchored (origin 6,6) or plain
        // (BtCoin), so each button is placed on its socket in the
        // background: the meso coin at (8, 267), the gather/sort pair in
        // the dark bottom-right panel and the expand toggle in the title
        // bar.
        inline Point<int16_t> drop_meso_pos()
        {
            return post_chaos() ? Point<int16_t>() : Point<int16_t>(8, 267);
        }

        inline Point<int16_t> gather_pos()
        {
            return post_chaos() ? Point<int16_t>() : Point<int16_t>(147, 273);
        }

        inline Point<int16_t> sort_pos()
        {
            return post_chaos() ? Point<int16_t>() : Point<int16_t>(162, 273);
        }

        inline Point<int16_t> expand_pos()
        {
            return post_chaos() ? Point<int16_t>() : Point<int16_t>(163, 11);
        }

        // Scrollbar. Post-Chaos arrows span y 50..248 at x 152; the v83
        // strip runs between the last socket column (x 148) and the border
        // (x 171), its arrows spanning y 50..240 at x 154.
        inline Range<int16_t> slider_vertical()
        {
            return post_chaos()
                ? Range<int16_t>(50, 248)
                : Range<int16_t>(50, 240);
        }

        inline int16_t slider_x()
        {
            return post_chaos() ? 152 : 154;
        }

        // Meso readout, right-aligned into the coin row.
        inline Point<int16_t> meso_label_pos()
        {
            return post_chaos()
                ? Point<int16_t>(124, 264)
                : Point<int16_t>(137, 268);
        }

        // Socket grid: four 32px cells per row, 36px apart. Post-Chaos
        // rows are 35px apart starting at (11, 51); v83 rows are 34px
        // apart starting at (8, 50).
        inline Point<int16_t> grid_origin()
        {
            return post_chaos()
                ? Point<int16_t>(11, 51)
                : Point<int16_t>(8, 50);
        }

        inline int16_t column_pitch()
        {
            return 36;
        }

        inline int16_t row_pitch()
        {
            return post_chaos() ? 35 : 34;
        }

        // Window-relative position of socket `absslot`, the 0-based offset
        // within the visible page.
        inline Point<int16_t> slotpos(int16_t absslot)
        {
            return Point<int16_t>(
                grid_origin().x() + (absslot % 4) * column_pitch(),
                grid_origin().y() + (absslot / 4) * row_pitch()
                );
        }

        // 0-based page offset of the socket under a window-relative cursor
        // position, or -1 when the cursor is outside the grid. The v83 hit
        // test is bounded by the last socket row (y 50 + 6*34 = 254) of
        // its shorter window; the upstream test leaves the body unbounded
        // below the first row.
        inline int16_t page_offset_at(Point<int16_t> cursorpos)
        {
            const int16_t xoff = cursorpos.x() - grid_origin().x();
            const int16_t yoff = cursorpos.y() - grid_origin().y();
            if (xoff < 1 || xoff > 143 || yoff < 1)
                return -1;
            if (!post_chaos() && yoff > 203)
                return -1;
            return (xoff / column_pitch()) + 4 * (yoff / row_pitch());
        }

        // "New item" flash (New/Tab0, 34x18) per tab. Post-Chaos flashes
        // sit at y 28 over the origin-bearing tab sprites; in v83 the
        // 27x16 sprites carry no origin, so the flash is centred over
        // each one. Note the strip order differs: v83's third sprite is
        // the ETC tab while post-Chaos puts SETUP there.
        inline Point<int16_t> tab_flash_pos(InventoryType::Id tb)
        {
            if (post_chaos())
            {
                switch (tb)
                {
                case InventoryType::EQUIP:
                    return Point<int16_t>(10, 28);
                case InventoryType::USE:
                    return Point<int16_t>(42, 28);
                case InventoryType::SETUP:
                    return Point<int16_t>(74, 28);
                case InventoryType::ETC:
                    return Point<int16_t>(105, 28);
                case InventoryType::CASH:
                    return Point<int16_t>(138, 28);
                default:
                    return Point<int16_t>();
                }
            }

            switch (tb)
            {
            case InventoryType::EQUIP:
                return Point<int16_t>(6, 23);
            case InventoryType::USE:
                return Point<int16_t>(38, 23);
            case InventoryType::SETUP:
                return Point<int16_t>(102, 23);
            case InventoryType::ETC:
                return Point<int16_t>(70, 23);
            case InventoryType::CASH:
                return Point<int16_t>(134, 23);
            default:
                return Point<int16_t>();
            }
        }
    }
}
