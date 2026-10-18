//////////////////////////////////////////////////////////////////////////////
// Screen geometry for the modern full-screen cash shop renderer.
//
// The CashShop.img bitmaps are authored for a 1024x768 viewport while the
// client canvas is a fixed 800x600. Native artwork sizes and cell pitches are
// mapped by 25/32 through px(), so bitmaps, the stretched background and the
// overlays placed on top of them share one pixel grid. Values that are pure
// screen-space choices are already in 800x600 canvas coordinates.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"

#include <cstddef>
#include <cstdint>

namespace jrc::cash_shop_layout
{
    constexpr int16_t VIEW_WIDTH = 800;
    constexpr int16_t VIEW_HEIGHT = 600;

    // Native 1024x768 pixels -> 800x600 screen pixels.
    constexpr int16_t px(int32_t native)
    {
        return static_cast<int16_t>((native * 25 + 16) / 32);
    }

    // Catalog cards are drawn 4 per row and 3 rows per page; the right pane
    // holds 7 columns of 3 slots. Both grids keep their native cell pitch.
    constexpr size_t GRID_COLS = 4;
    constexpr size_t SLOT_COLS = 7;

    // Visible height of an A11M line and the offset from the passed y to its
    // first lit row; used to centre labels inside controls.
    constexpr int16_t TEXT_HEIGHT = 10;
    constexpr int16_t TEXT_BASELINE_SHIFT = 5;

    // --- Header ----------------------------------------------------------
    // Title, three currency readouts and the two action pills.
    constexpr Point<int16_t> TITLE_POS{ 109, 3 };
    constexpr Point<int16_t> TOP_BAR_POS{ 100, 0 };
    constexpr Point<int16_t> TOP_BAR_SIZE{ 700, 32 };
    constexpr int16_t BALANCE_LABEL_XS[3]{ 200, 310, 420 };
    constexpr int16_t BALANCE_VALUE_RIGHT_XS[3]{ 292, 402, 512 };
    constexpr int16_t BALANCE_Y = 11;
    constexpr Point<int16_t> CHARGE_POS{ 606, 6 };
    constexpr Point<int16_t> CHARGE_SIZE{ 72, 20 };
    constexpr Point<int16_t> EXIT_POS{ 700, 5 };
    constexpr Point<int16_t> EXIT_SIZE{ 90, 23 };

    // --- Left rail -------------------------------------------------------
    // The bright 0..98 strip of the painted background holds the categories.
    constexpr Point<int16_t> CATEGORY_TITLE_POS{ 10, 0 };
    constexpr Point<int16_t> CATEGORY_POS{ 4, 40 };
    constexpr Point<int16_t> CATEGORY_SIZE{ 90, 24 };
    constexpr int16_t CATEGORY_STEP_Y = 34;
    constexpr Point<int16_t> CATEGORY_LABEL_OFFSET{ 0, 1 };
    constexpr Point<int16_t> CATEGORY_LINE_OFFSET{ 0, 21 };
    constexpr Point<int16_t> CATEGORY_LINE_SIZE{ 90, 2 };

    // --- Catalog ---------------------------------------------------------
    // Native search bar at (140,44), 491x32, matching the card row width.
    constexpr Point<int16_t> SEARCH_BAR_POS{ 109, 34 };
    constexpr Point<int16_t> SEARCH_BAR_SIZE{ 384, 25 };
    constexpr Point<int16_t> SEARCH_LABEL_POS{ 114, 36 };
    // Canvas-space rect drawn without draw_position(): Textfield::draw()
    // reports these bounds to the IME verbatim, so it must stay unshifted.
    constexpr Rectangle<int16_t> SEARCH_FIELD_BOUNDS{
        { 156, 26 }, { 480, 44 } };
    constexpr Rectangle<int16_t> SEARCH_HITBOX{
        { 109, 34 }, { 493, 59 } };

    // Payment currency chips under the search bar.
    constexpr Point<int16_t> CURRENCY_POS{ 109, 66 };
    constexpr Point<int16_t> CURRENCY_SIZE{ 48, 16 };
    constexpr int16_t CURRENCY_STEP_X = 52;

    // Native card grid at (140,110) with 119x184 cards and a 124/189 pitch.
    constexpr Point<int16_t> GRID_POS{ 109, 86 };
    constexpr int16_t GRID_STEP_X = 97;
    constexpr int16_t GRID_STEP_Y = 148;
    constexpr Point<int16_t> CARD_SIZE{ 93, 144 };
    constexpr Point<int16_t> CARD_ICON_CENTER{ 46, 34 };
    constexpr int16_t CARD_ICON_MAX = 56;
    constexpr Point<int16_t> CARD_NAME_POS{ 46, 67 };
    constexpr int16_t CARD_NAME_WIDTH = 88;
    constexpr Point<int16_t> CARD_PRICE_ICON_POS{ 19, 84 };
    constexpr Point<int16_t> CARD_MARKER_CENTER{ 8, 8 };
    constexpr Point<int16_t> CARD_BUY_OFFSET{ 7, 117 };
    constexpr Point<int16_t> CARD_BUY_SIZE{ 79, 19 };

    // Page placeholders are centred on the catalog grid.
    constexpr Point<int16_t> EMPTY_CATALOG_CENTER{ 301, 306 };
    constexpr int16_t EMPTY_HINT_WIDTH = 280;
    constexpr Point<int16_t> EMPTY_HINT_SIZE{ px(348), px(148) };

    // Left pager (native 30x47 arrows at y680).
    constexpr Point<int16_t> LEFT_PREV_POS{ 109, 534 };
    constexpr Point<int16_t> LEFT_NEXT_POS{ 463, 534 };
    constexpr Point<int16_t> LEFT_PAGE_POS{ 301, 542 };
    constexpr int16_t LEFT_PAGE_WIDTH = 160;
    constexpr Point<int16_t> PAGER_SIZE{ px(30), px(47) };

    // --- Right pane ------------------------------------------------------
    // Native inventory panel at (635,73), 380x213, 46x46 slots with 50 pitch
    // starting (10,57) inside the panel.
    constexpr Point<int16_t> TAB_LOCKER_POS{ 496, 34 };
    constexpr Point<int16_t> TAB_INVENTORY_POS{ 646, 34 };
    constexpr Point<int16_t> TAB_SIZE{ 147, 18 };
    constexpr int16_t TAB_LABEL_WIDTH = TAB_SIZE.x();
    constexpr Point<int16_t> TAB_BAR_POS{ 496, 54 };
    constexpr Point<int16_t> TAB_BAR_SIZE{ 297, 2 };
    constexpr Point<int16_t> PANEL_POS{ 496, 57 };
    constexpr Point<int16_t> PANEL_SIZE{ 297, 166 };
    constexpr Point<int16_t> PANE_TITLE_POS{ 508, 66 };
    constexpr Point<int16_t> PANE_INFO_RIGHT{ 781, 69 };
    constexpr Point<int16_t> SLOT_GRID_POS{ 504, 102 };
    constexpr int16_t SLOT_STEP = 39;
    constexpr Point<int16_t> SLOT_SIZE{ 36, 36 };
    constexpr int16_t SLOT_ICON_MAX = 34;
    constexpr Point<int16_t> SLOT_COUNT_OFFSET{ 33, 19 };
    constexpr Point<int16_t> SLOT_MARKER_CENTER{ 7, 7 };
    constexpr Point<int16_t> EMPTY_RIGHT_CENTER{ 644, 162 };

    // Footer under the panel: pager, page label and the transfer action.
    constexpr Point<int16_t> RIGHT_PREV_POS{ 496, 228 };
    constexpr Point<int16_t> RIGHT_NEXT_POS{ 761, 228 };
    constexpr Point<int16_t> RIGHT_PAGE_POS{ 630, 236 };
    constexpr int16_t RIGHT_PAGE_WIDTH = 100;
    constexpr Point<int16_t> TRANSFER_POS{ 686, 241 };
    constexpr Point<int16_t> TRANSFER_SIZE{ 64, 16 };
    constexpr Point<int16_t> TRANSFER_HINT_POS{ 644, 281 };
    constexpr int16_t TRANSFER_HINT_WIDTH = 290;

    // Selected item details between the footer and the bottom of the pane.
    constexpr Point<int16_t> DETAILS_PANEL_POS{ 496, 300 };
    constexpr Point<int16_t> DETAILS_PANEL_SIZE{ 297, 118 };
    constexpr Point<int16_t> DETAILS_LINE_POS{ 496, 300 };
    constexpr Point<int16_t> DETAILS_LINE_SIZE{ 297, 2 };
    constexpr Point<int16_t> DETAILS_ICON_CENTER{ 546, 358 };
    constexpr int16_t DETAILS_ICON_MAX = 84;
    constexpr Point<int16_t> DETAILS_TEXT_CENTER{ 700, 322 };
    constexpr int16_t DETAILS_TEXT_WIDTH = 150;
    constexpr int16_t DETAILS_PRICE_Y = 346;
    constexpr int16_t DETAILS_COUNT_Y = 370;
    constexpr Point<int16_t> DETAILS_HINT_CENTER{ 644, 348 };
    constexpr int16_t DETAILS_HINT_WIDTH = 280;

    // Cursor x that separates the catalog from the right pane when scrolling.
    constexpr int16_t SCROLL_SPLIT_X = 495;

    // Top-left corner of the n-th category entry.
    inline Point<int16_t> category_position(int8_t index)
    {
        return {
            CATEGORY_POS.x(),
            static_cast<int16_t>(CATEGORY_POS.y() + index * CATEGORY_STEP_Y)
        };
    }

    // Top-left corner of the n-th currency chip.
    inline Point<int16_t> currency_position(size_t index)
    {
        return {
            static_cast<int16_t>(
                CURRENCY_POS.x() + index * CURRENCY_STEP_X),
            CURRENCY_POS.y()
        };
    }

    // Top-left corner of the n-th catalog card on a page.
    inline Point<int16_t> card_position(size_t index)
    {
        return {
            static_cast<int16_t>(GRID_POS.x() + (index % GRID_COLS) * GRID_STEP_X),
            static_cast<int16_t>(GRID_POS.y() + (index / GRID_COLS) * GRID_STEP_Y)
        };
    }

    // Top-left corner of the n-th inventory slot on a page.
    inline Point<int16_t> slot_position(size_t index)
    {
        return {
            static_cast<int16_t>(SLOT_GRID_POS.x() + (index % SLOT_COLS) * SLOT_STEP),
            static_cast<int16_t>(SLOT_GRID_POS.y() + (index / SLOT_COLS) * SLOT_STEP)
        };
    }

    // Centre of a pill's label line. The visible A11M line sits below the
    // passed y, so the baseline shift moves the text back onto the centre.
    inline Point<int16_t> pill_label(
        Point<int16_t> pos, Point<int16_t> size)
    {
        return {
            static_cast<int16_t>(pos.x() + size.x() / 2),
            static_cast<int16_t>(pos.y() + (size.y() - TEXT_HEIGHT) / 2 -
                TEXT_BASELINE_SHIFT)
        };
    }
}
