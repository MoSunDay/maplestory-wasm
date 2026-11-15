//////////////////////////////////////////////////////////////////////////////
// Layout math for the 2023 (modern) character select screen.
//
// The screen keeps the proven roster anchors - a single row of four
// characters on disc platforms with page arrows flanking it - and wraps
// them in flat 2023-style chrome: a top bar, a translucent left stage
// band, a right info card with four rows, a START button under the card
// and a footer bar behind the create/delete buttons. NX art is only an
// accent layer on top of that chrome (world banner, slot count window,
// nametags, the lv charset, page arrows and the button bitmaps), so the
// pure constants and free functions below fully describe the geometry.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include <cstddef>
#include <cstdint>

namespace jrc
{
    namespace charselect_modern
    {
        // --- Roster (unchanged single row) ---------------------------------
        constexpr uint8_t PAGESIZE = 4;

        // character/0 is a disc whose origin sinks the feet into the art;
        // shifting it down puts the top face at feet level. The nametag
        // then clears the disc body below the feet anchor.
        constexpr Point<int16_t> PLATFORM_OFFSET{ 0, 7 };
        constexpr Point<int16_t> NAMETAG_OFFSET{ 0, 19 };

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

        // --- Top bar --------------------------------------------------------
        // Full-width dark band above the world banner stack; the 2px orange
        // accent on its bottom edge is the modern chrome signature.
        constexpr Point<int16_t> TOPBAR_POS{ 12, 8 };
        constexpr Point<int16_t> TOPBAR_SIZE{ 776, 32 };
        constexpr int16_t TOPBAR_ACCENT_HEIGHT = 2;

        // NX accent: Common/selectWorld board + selectedWorld overlay art,
        // kept at the post-Chaos banner anchor directly under the bar.
        constexpr Point<int16_t> WORLD_BANNER_POS{ 578, 42 };

        // Slot count label fallback when the NX charSlot window is absent:
        // right aligned inside the top bar.
        constexpr Point<int16_t> COUNT_FALLBACK_POS{ 766, 19 };

        // --- Left stage band ------------------------------------------------
        // Translucent panel behind the roster row; the page arrows sit
        // inside it at the flanks of the card grid.
        constexpr Point<int16_t> STAGE_POS{ 16, 150 };
        constexpr Point<int16_t> STAGE_SIZE{ 544, 170 };

        constexpr Point<int16_t> PAGE_ARROW_LEFT_POS{ 46, 240 };
        constexpr Point<int16_t> PAGE_ARROW_RIGHT_POS{ 533, 240 };

        // Selected roster slot: orange outline around the hotzone inflated
        // by this margin.
        constexpr int16_t SLOT_SELECT_MARGIN = 3;

        // --- Right info card (four rows) -------------------------------------
        // Row 0 is the header (name + Lv), rows 1..3 are data pills:
        // job on a full-width row, then two 2-column stat rows.
        constexpr Point<int16_t> CARD_POS{ 576, 150 };
        constexpr Point<int16_t> CARD_SIZE{ 212, 170 };

        constexpr int16_t CARD_HEADER_BOTTOM = 206;

        constexpr Point<int16_t> CARD_JOB_ROW_POS{ 584, 214 };
        constexpr Point<int16_t> CARD_JOB_ROW_SIZE{ 196, 28 };
        constexpr Point<int16_t> CARD_STAT_ROW_POS{ 584, 248 };
        constexpr Point<int16_t> CARD_STAT_ROW_SIZE{ 196, 28 };
        constexpr Point<int16_t> CARD_STAT_ROW2_POS{ 584, 282 };
        constexpr int16_t CARD_CELL_INNER_X = 8;
        constexpr int16_t CARD_CELL_HALF = 96;
        constexpr int16_t CARD_CELL_GAP = 4;

        // Name (A18M centered) and Lv charset anchor in the header row.
        constexpr Point<int16_t> CARD_NAME_POS{ 682, 157 };
        constexpr Point<int16_t> CARD_LV_POS{ 682, 201 };

        // --- START button -----------------------------------------------------
        // BtSelect art is 140x52, centred under the info card.
        constexpr Point<int16_t> START_POS{ 612, 336 };

        // --- Footer bar --------------------------------------------------------
        // Dark band behind the create/delete buttons (110x37 art each).
        constexpr Point<int16_t> FOOTER_POS{ 16, 478 };
        constexpr Point<int16_t> FOOTER_SIZE{ 436, 62 };

        constexpr Point<int16_t> CREATE_POS{ 200, 495 };
        constexpr Point<int16_t> DELETE_POS{ 320, 495 };

        // --- Card content anchors ------------------------------------------------
        // Info label slots in UICharSelect::InfoLabel order. Values are
        // right aligned A11M lines; x marks the right edge of the value.
        enum CardLabel
        {
            CARD_LABEL_JOB,
            CARD_LABEL_STR,
            CARD_LABEL_INT,
            CARD_LABEL_DEX,
            CARD_LABEL_LUK
        };

        constexpr int16_t CARD_ROW_TEXT_TOP_OFFSET = 9;

        constexpr Point<int16_t> card_label_pos(size_t label)
        {
            switch (label)
            {
            case CARD_LABEL_JOB:
                return {
                    static_cast<int16_t>(
                        CARD_JOB_ROW_POS.x() + CARD_JOB_ROW_SIZE.x() -
                        CARD_CELL_INNER_X),
                    static_cast<int16_t>(CARD_JOB_ROW_POS.y() +
                        CARD_ROW_TEXT_TOP_OFFSET)
                };
            case CARD_LABEL_STR:
                return {
                    static_cast<int16_t>(CARD_STAT_ROW_POS.x() +
                        CARD_CELL_HALF - CARD_CELL_INNER_X),
                    static_cast<int16_t>(CARD_STAT_ROW_POS.y() +
                        CARD_ROW_TEXT_TOP_OFFSET)
                };
            case CARD_LABEL_INT:
                return {
                    static_cast<int16_t>(CARD_STAT_ROW_POS.x() +
                        CARD_CELL_INNER_X + CARD_CELL_GAP + CARD_CELL_HALF -
                        CARD_CELL_INNER_X),
                    static_cast<int16_t>(CARD_STAT_ROW_POS.y() +
                        CARD_ROW_TEXT_TOP_OFFSET)
                };
            case CARD_LABEL_DEX:
                return {
                    static_cast<int16_t>(CARD_STAT_ROW_POS.x() +
                        CARD_CELL_HALF - CARD_CELL_INNER_X),
                    static_cast<int16_t>(CARD_STAT_ROW2_POS.y() +
                        CARD_ROW_TEXT_TOP_OFFSET)
                };
            case CARD_LABEL_LUK:
                return {
                    static_cast<int16_t>(CARD_STAT_ROW_POS.x() +
                        CARD_CELL_INNER_X + CARD_CELL_GAP + CARD_CELL_HALF -
                        CARD_CELL_INNER_X),
                    static_cast<int16_t>(CARD_STAT_ROW2_POS.y() +
                        CARD_ROW_TEXT_TOP_OFFSET)
                };
            default:
                return {};
            }
        }
    }
}
