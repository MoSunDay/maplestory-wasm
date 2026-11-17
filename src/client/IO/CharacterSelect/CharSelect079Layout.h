//////////////////////////////////////////////////////////////////////////////
// Layout math for the 079MAX4 bitmap character-select screen.
//
// The 079 assets are authored for the original 800x600 login canvas: the
// 38 parallax layers and the 800x600 frame both anchor at (400,300), the
// roster sits on one row of disc platforms at the classic pre-Chaos feet
// anchors, and the 161x188 info card hangs off the right side at (625,305).
//
// charInfo1 is 161x188 with origin (45,57), so anchoring it at (625,305)
// puts its top-left at (580,248). Everything inside the card is a baked
// caption band, measured in card local coordinates:
//   y3..17     blue header, white A12M name centred on x80 (glyph top y5)
//   y26..36    job row, its label spans x16..38
//   y45..55    level/fame row, labels span x16..38 and x84..118
//   y64..74    str/int row
//   y83..93    dex/luk row
//   y99..114   rank caption band, light value band y117..134 below it
//   y136..151  jobrank caption band, light value band y155..172 below it
// Only the values are drawn on top of those captions. A11M text has
// linespace 14 and bitmap top offset 3, so a value whose glyph top must
// land on local y is drawn at y = 248 + y - 11; A12M has linespace 17, so
// its draw y is 248 + y - 14. Text::RIGHT values are anchored at their
// right edge and Text::CENTER values on the column centre.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include <cstddef>
#include <cstdint>

namespace jrc
{
    namespace charselect_079
    {
        constexpr uint8_t PAGESIZE = 4;

        // --- Backdrop and frame -------------------------------------------
        constexpr Point<int16_t> CANVAS_SIZE{ 800, 600 };
        constexpr Point<int16_t> BACKDROP_POS{ 400, 300 };
        constexpr Point<int16_t> FRAME_POS{ 400, 300 };

        // The 150x57 world banner board injected as Login.img/selectWorld079.
        constexpr Point<int16_t> WORLD_BANNER_POS{ 578, 112 };

        // The frame's transparent interior spans x41-756 / y40-488.
        constexpr int16_t FRAME_INNER_LEFT = 41;
        constexpr int16_t FRAME_INNER_RIGHT = 756;
        constexpr int16_t FRAME_INNER_TOP = 40;
        constexpr int16_t FRAME_INNER_BOTTOM = 488;

        // --- Roster ---------------------------------------------------------
        // The 079 character/0 disc already sits at the feet anchor, so the
        // platform and preview share one position. The 20px nametag hangs
        // from that anchor with its top two pixels shifted down by the
        // Nametag helper.
        constexpr Point<int16_t> char_slot_pos(size_t i)
        {
            return { static_cast<int16_t>(130 + 120 * i), 270 };
        }

        constexpr Point<int16_t> slot_hotzone_pos(size_t i)
        {
            return { static_cast<int16_t>(100 + 120 * i), 160 };
        }

        constexpr Point<int16_t> slot_hotzone_dim()
        {
            return { 60, 120 };
        }

        // --- Info card ------------------------------------------------------
        // Anchor (625,305) with the card top-left at (580,248); each
        // position is that top-left plus the measured glyph/bitmap top.
        constexpr Point<int16_t> INFO_ANCHOR{ 625, 305 };
        constexpr Point<int16_t> INFO_TOPLEFT{ 580, 248 };

        // A12M centred white name in the blue header; glyph top y5.
        constexpr Point<int16_t> INFO_NAME_POS{ 660, 239 };
        // Job-name bitmap (Texture origin 0,0) top-left at local (44,26).
        constexpr Point<int16_t> INFO_JOB_ICON_POS{ 624, 274 };
        // A11M LEFT text fallback; draw y puts the glyph top at local y26.
        constexpr Point<int16_t> INFO_JOB_POS{ 624, 263 };
        // A11M RIGHT values; glyph top y45, y64 and y83 respectively.
        constexpr Point<int16_t> INFO_LEVEL_POS{ 660, 282 };
        constexpr Point<int16_t> INFO_FAME_POS{ 730, 282 };
        constexpr Point<int16_t> INFO_STR_POS{ 660, 301 };
        constexpr Point<int16_t> INFO_INT_POS{ 730, 301 };
        constexpr Point<int16_t> INFO_DEX_POS{ 660, 320 };
        constexpr Point<int16_t> INFO_LUK_POS{ 730, 320 };
        // A11M CENTER values inside the light bands below the two caption
        // bands; glyph tops at local y121 and y159.
        constexpr Point<int16_t> INFO_RANK_POS{ 660, 358 };
        constexpr Point<int16_t> INFO_JOBRANK_POS{ 660, 396 };

        // The small movement arrow rides right of the centred rank value.
        // The 11px/17px icons hang lower than the 9px ones so their baselines
        // line up with the 11px-tall row text.
        constexpr Point<int16_t> rank_icon_pos(
            Point<int16_t> value_pos, int16_t value_width, int16_t icon_height)
        {
            return {
                static_cast<int16_t>(value_pos.x() + value_width / 2 + 4),
                static_cast<int16_t>(value_pos.y() + (icon_height >= 9 ? 11 : 14))
            };
        }

        // --- Buttons and counters ------------------------------------------
        constexpr Point<int16_t> START_POS{ 440, 495 };
        constexpr Point<int16_t> CREATE_POS{ 200, 495 };
        constexpr Point<int16_t> DELETE_POS{ 320, 495 };
        constexpr Point<int16_t> PAGE_LEFT_POS{ 100, 490 };
        constexpr Point<int16_t> PAGE_RIGHT_POS{ 698, 490 };
        constexpr Point<int16_t> COUNT_POS{ 722, 183 };
    }
}
