//////////////////////////////////////////////////////////////////////////////
// Screen geometry for the modern full-screen cash shop renderer.
//
// The CashShop.img bitmaps are authored for a 1024x768 viewport while the
// client canvas is an 800x600 design surface. Native artwork sizes and
// cell pitches are mapped by 25/32 through px(), so bitmaps, the stretched
// background and the overlays placed on top of them share one pixel grid.
// Values that are pure screen-space choices are already in 800x600 canvas
// coordinates. The design canvas scales uniformly to fill taller viewports
// (1.2x at 1280x720), and origin() centres the scaled canvas; at exactly
// 800x600 nothing is scaled or shifted.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Constants.h"
#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"
#include "CashShopView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace jrc::cash_shop_layout
{
    constexpr int16_t VIEW_WIDTH = 800;
    constexpr int16_t VIEW_HEIGHT = 600;

    // Uniform factor applied to the 800x600 design canvas so it fills taller
    // viewports; it is 1.0f when the viewport is 800x600 or smaller.
    inline float content_scale()
    {
        const float width = static_cast<float>(Constants::viewwidth());
        const float height = static_cast<float>(Constants::viewheight());
        return std::max(1.0f,
            std::min(width / VIEW_WIDTH, height / VIEW_HEIGHT));
    }

    // Design-canvas coordinate rounded onto the scaled grid.
    inline int16_t scale_coord(int16_t value)
    {
        return static_cast<int16_t>(
            std::lround(value * content_scale()));
    }

    // Design-canvas point scaled on both axes.
    inline Point<int16_t> scale_point(Point<int16_t> value)
    {
        return { scale_coord(value.x()), scale_coord(value.y()) };
    }

    // Top-left corner of the scaled design canvas inside the current
    // viewport. It is zero when the viewport is exactly 800x600.
    inline Point<int16_t> origin()
    {
        const int16_t x = static_cast<int16_t>(std::max(0,
            (Constants::viewwidth() - scale_coord(VIEW_WIDTH)) / 2));
        const int16_t y = static_cast<int16_t>(std::max(0,
            (Constants::viewheight() - scale_coord(VIEW_HEIGHT)) / 2));
        return Point<int16_t>(x, y);
    }

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
    // Title, three currency readouts and the two action pills. The balance
    // columns are 140px apart so a nine-digit value never reaches the label
    // on its left; the action pair is anchored ten pixels from the edge.
    constexpr Point<int16_t> TITLE_POS{ 12, 4 };
    constexpr Point<int16_t> TOP_BAR_POS{ 0, 0 };
    constexpr Point<int16_t> TOP_BAR_SIZE{ VIEW_WIDTH, 32 };
    constexpr int16_t BALANCE_ICON_XS[3]{ 100, 240, 380 };
    constexpr int16_t BALANCE_ICON_Y = 7;
    constexpr int16_t BALANCE_LABEL_XS[3]{ 116, 256, 396 };
    constexpr int16_t BALANCE_VALUE_RIGHT_XS[3]{ 232, 372, 512 };
    constexpr int16_t BALANCE_Y = 11;
    constexpr Point<int16_t> CHARGE_POS{ 606, 6 };
    constexpr Point<int16_t> CHARGE_SIZE{ 72, 20 };
    constexpr Point<int16_t> EXIT_POS{ 698, 6 };
    constexpr Point<int16_t> EXIT_SIZE{ 92, 20 };

    // --- Left rail -------------------------------------------------------
    // One translucent 90px rail runs from below the header to the footer.
    // Primary categories occupy 8 x 22px rows; the lower half is a quiet
    // badge legend. Opening the equipment sub-menu swaps the rail content
    // in place instead of covering the catalog.
    constexpr Point<int16_t> RAIL_PANEL_POS{ 4, 34 };
    constexpr Point<int16_t> RAIL_PANEL_SIZE{ 90, 512 };
    constexpr Point<int16_t> CATEGORY_TITLE_POS{ 10, 38 };
    constexpr Point<int16_t> CATEGORY_POS{ 4, 58 };
    constexpr Point<int16_t> CATEGORY_SIZE{ 90, 22 };
    constexpr int16_t CATEGORY_STEP_Y = 22;
    constexpr Point<int16_t> CATEGORY_ICON_CENTER{ 16, 11 };
    constexpr int16_t CATEGORY_ICON_MAX = 16;
    constexpr Point<int16_t> CATEGORY_LABEL_OFFSET{ 34, 0 };
    constexpr int16_t CATEGORY_LABEL_WIDTH = 26;
    constexpr Point<int16_t> CATEGORY_COUNT_OFFSET{ 84, 0 };
    constexpr int16_t CATEGORY_COUNT_WIDTH = 40;
    constexpr Point<int16_t> CATEGORY_LINE_OFFSET{ 0, 0 };
    constexpr Point<int16_t> CATEGORY_LINE_SIZE{ 3, 22 };
    constexpr Point<int16_t> CATEGORY_SECTION_LINE_POS{ 10, 241 };
    constexpr Point<int16_t> CATEGORY_SECTION_LINE_SIZE{ 80, 1 };

    // --- Equipment sub-menu ----------------------------------------------
    // The sub-menu replaces the primary rows. Back lives in the rail title
    // row; the 17 sub categories use compact 21px rows, and the space below
    // becomes a live filter summary.
    constexpr Point<int16_t> EQSUB_POS{ 4, 60 };
    constexpr int16_t EQSUB_WIDTH = 90;
    constexpr int16_t EQSUB_ENTRY_STEP_Y = 21;
    constexpr Point<int16_t> EQSUB_ENTRY_SIZE{ EQSUB_WIDTH, EQSUB_ENTRY_STEP_Y };
    constexpr Point<int16_t> EQSUB_TITLE_POS{ 10, 40 };
    constexpr Point<int16_t> EQSUB_BACK_POS{ 88, 40 };
    constexpr Point<int16_t> EQSUB_LABEL_OFFSET{ 12, 0 };
    constexpr int16_t EQSUB_LABEL_WIDTH = 46;
    constexpr Point<int16_t> EQSUB_COUNT_OFFSET{ 84, 0 };
    constexpr int16_t EQSUB_COUNT_WIDTH = 40;
    constexpr Point<int16_t> EQSUB_CLOSE_POS{ 4, 36 };
    constexpr Point<int16_t> EQSUB_CLOSE_SIZE{ 90, 22 };
    constexpr Point<int16_t> EQSUB_SECTION_LINE_POS{ 10, 423 };
    constexpr Point<int16_t> EQSUB_SECTION_LINE_SIZE{ 80, 1 };
    constexpr Point<int16_t> EQSUB_STATUS_PANEL_POS{ 6, 430 };
    constexpr Point<int16_t> EQSUB_STATUS_PANEL_SIZE{ 86, 108 };
    constexpr Point<int16_t> EQSUB_STATUS_POS{ 10, 436 };
    constexpr int16_t EQSUB_STATUS_WIDTH = 78;

    // Badge legend in the lower half of the rail. It is plain content on the
    // shared rail surface, not a second nested panel.
    constexpr Point<int16_t> LEGEND_TITLE_POS{ 10, 246 };
    constexpr Point<int16_t> LEGEND_ROW_POS{ 10, 266 };
    constexpr int16_t LEGEND_ROW_STEP = 29;
    constexpr Point<int16_t> LEGEND_BADGE_SIZE{ 24, 22 };
    constexpr Point<int16_t> LEGEND_LABEL_OFFSET{ 32, 4 };
    constexpr int16_t LEGEND_LABEL_WIDTH = 50;
    constexpr Point<int16_t> LEGEND_NOTE_SEPARATOR_POS{ 10, 412 };
    constexpr Point<int16_t> LEGEND_NOTE_SEPARATOR_SIZE{ 80, 1 };
    constexpr Point<int16_t> LEGEND_NOTE_POS{ 8, 420 };
    constexpr int16_t LEGEND_NOTE_WIDTH = 82;
    constexpr Point<int16_t> RAIL_HELP_POS{ 8, 466 };
    constexpr int16_t RAIL_HELP_WIDTH = 82;

    // --- Catalog ---------------------------------------------------------
    // Left content is a strict 100..496 column. The search bar, currency
    // row, card grid and footer rule share that width; the right pane starts
    // at 508, leaving a consistent 12px gutter.
    constexpr Point<int16_t> SEARCH_BAR_POS{ 100, 34 };
    constexpr Point<int16_t> SEARCH_BAR_SIZE{ 396, 25 };
    constexpr Point<int16_t> SEARCH_LABEL_POS{ 105, 36 };
    // Canvas-space rect drawn without draw_position(): Textfield::draw()
    // reports these bounds to the IME verbatim, so it must stay unshifted.
    constexpr Rectangle<int16_t> SEARCH_FIELD_BOUNDS{
        { 148, 36 }, { 470, 54 } };
    constexpr Rectangle<int16_t> SEARCH_HITBOX{
        { 100, 34 }, { 471, 59 } };
    constexpr Point<int16_t> SEARCH_CLEAR_POS{ 472, 36 };
    constexpr Point<int16_t> SEARCH_CLEAR_SIZE{ 16, 21 };

    // Viewport-space search hitbox: the scaled design rect moved by origin().
    inline Rectangle<int16_t> search_hitbox()
    {
        const Point<int16_t> offset = origin();
        return {
            scale_point(SEARCH_HITBOX.getlt()) + offset,
            scale_point(SEARCH_HITBOX.getrb()) + offset
        };
    }

    // Payment currency chips under the search bar.
    constexpr Point<int16_t> CURRENCY_POS{ 100, 64 };
    constexpr Point<int16_t> CURRENCY_SIZE{ 48, 16 };
    constexpr int16_t CURRENCY_STEP_X = 52;

    // Filter summary at the right end of the currency row.
    constexpr Point<int16_t> CATALOG_INFO_BOX_POS{ 268, 64 };
    constexpr Point<int16_t> CATALOG_INFO_BOX_SIZE{ 228, 18 };
    constexpr Point<int16_t> CATALOG_INFO_POS{ 492, 66 };
    constexpr int16_t CATALOG_INFO_WIDTH = 220;

    // Four 93px cards on a 101px pitch: the last card ends at x=496.
    constexpr Point<int16_t> GRID_POS{ 100, 88 };
    constexpr int16_t GRID_STEP_X = 101;
    constexpr int16_t GRID_STEP_Y = 148;
    constexpr Point<int16_t> CARD_SIZE{ 93, 144 };
    constexpr Point<int16_t> CARD_ICON_CENTER{ 42, 40 };
    constexpr int16_t CARD_ICON_MAX = 44;
    constexpr Point<int16_t> CARD_NAME_POS{ 46, 67 };
    constexpr int16_t CARD_NAME_WIDTH = 88;
    constexpr Point<int16_t> CARD_META_POS{ 46, 99 };
    constexpr int16_t CARD_META_WIDTH = 88;
    constexpr Point<int16_t> CARD_BADGE_POS{ 65, 1 };
    constexpr Point<int16_t> CARD_BADGE_SIZE{ 24, 22 };
    constexpr Point<int16_t> CARD_PRICE_ICON_POS{ 19, 84 };
    constexpr Point<int16_t> CARD_MARKER_CENTER{ 8, 8 };
    constexpr Point<int16_t> CARD_BUY_OFFSET{ 7, 117 };
    constexpr Point<int16_t> CARD_BUY_SIZE{ 79, 19 };

    // Page placeholders are centred on the catalog column.
    constexpr Point<int16_t> EMPTY_CATALOG_CENTER{ 298, 310 };
    constexpr Point<int16_t> EMPTY_HINT_SIZE{ px(348), px(148) };
    constexpr Point<int16_t> EMPTY_CATALOG_LABEL_POS{ 298, 378 };
    constexpr int16_t EMPTY_CATALOG_LABEL_WIDTH = 300;

    // Left pager (native 30x47 arrows).
    constexpr Point<int16_t> LEFT_PREV_POS{ 100, 534 };
    constexpr Point<int16_t> LEFT_NEXT_POS{ 473, 534 };
    constexpr Point<int16_t> LEFT_PAGE_POS{ 298, 542 };
    constexpr int16_t LEFT_PAGE_WIDTH = 160;
    constexpr Point<int16_t> PAGER_SIZE{ px(30), px(47) };

    // Footer rule and hint under the catalog grid.
    constexpr Point<int16_t> CATALOG_LINE_POS{ 100, 575 };
    constexpr Point<int16_t> CATALOG_LINE_SIZE{ 396, 1 };
    constexpr Point<int16_t> CATALOG_HINT_POS{ 298, 579 };
    constexpr int16_t CATALOG_HINT_WIDTH = 396;

    // --- Right pane ------------------------------------------------------
    // Two 142px tabs, a 288px panel and an inner slot grid centred with
    // nine pixels on both sides (517..787 inside 508..796).
    constexpr Point<int16_t> TAB_LOCKER_POS{ 508, 34 };
    constexpr Point<int16_t> TAB_INVENTORY_POS{ 654, 34 };
    constexpr Point<int16_t> TAB_SIZE{ 142, 18 };
    constexpr int16_t TAB_LABEL_WIDTH = TAB_SIZE.x();
    constexpr Point<int16_t> TAB_BAR_POS{ 508, 54 };
    constexpr Point<int16_t> TAB_BAR_SIZE{ 288, 2 };
    constexpr Point<int16_t> PANEL_POS{ 508, 57 };
    constexpr Point<int16_t> PANEL_SIZE{ 288, 166 };
    constexpr Point<int16_t> PANE_TITLE_POS{ 516, 64 };
    constexpr Point<int16_t> PANE_INFO_RIGHT{ 788, 64 };
    constexpr Point<int16_t> SLOT_GRID_POS{ 517, 88 };
    constexpr int16_t SLOT_STEP = 39;
    constexpr Point<int16_t> SLOT_SIZE{ 36, 36 };
    constexpr int16_t SLOT_ICON_MAX = 34;
    constexpr Point<int16_t> SLOT_COUNT_OFFSET{ 33, 19 };
    constexpr Point<int16_t> SLOT_MARKER_CENTER{ 7, 7 };
    // Canvas size of the 346x146 slot-grid cover bitmap.
    constexpr Point<int16_t> SLOT_COVER_SIZE{ px(346), px(146) };
    constexpr Point<int16_t> EMPTY_RIGHT_CENTER{ 652, 145 };

    // Footer under the panel: pager, page label and the transfer action.
    constexpr Point<int16_t> RIGHT_PREV_POS{ 508, 228 };
    constexpr Point<int16_t> RIGHT_NEXT_POS{ 773, 228 };
    constexpr Point<int16_t> RIGHT_PAGE_POS{ 612, 236 };
    constexpr int16_t RIGHT_PAGE_WIDTH = 96;
    constexpr Point<int16_t> TRANSFER_POS{ 680, 239 };
    constexpr Point<int16_t> TRANSFER_SIZE{ 64, 16 };
    constexpr Point<int16_t> TRANSFER_HINT_POS{ 652, 270 };
    constexpr int16_t TRANSFER_HINT_WIDTH = 280;

    // Selected item details: a 22px header and one icon/three-line block.
    constexpr Point<int16_t> DETAILS_PANEL_POS{ 508, 294 };
    constexpr Point<int16_t> DETAILS_PANEL_SIZE{ 288, 94 };
    constexpr Point<int16_t> DETAILS_HEADER_LINE_POS{ 508, 316 };
    constexpr Point<int16_t> DETAILS_HEADER_LINE_SIZE{ 288, 1 };
    constexpr Point<int16_t> DETAILS_TITLE_POS{ 516, 298 };
    constexpr Point<int16_t> DETAILS_ICON_CENTER{ 552, 352 };
    constexpr int16_t DETAILS_ICON_MAX = 62;
    constexpr Point<int16_t> DETAILS_TEXT_CENTER{ 694, 318 };
    constexpr int16_t DETAILS_TEXT_WIDTH = 178;
    constexpr int16_t DETAILS_COUNT_Y = 334;
    constexpr int16_t DETAILS_META_Y = 348;
    constexpr int16_t DETAILS_EXTRA_Y = 362;
    constexpr Point<int16_t> DETAILS_HINT_CENTER{ 652, 342 };
    constexpr int16_t DETAILS_HINT_WIDTH = 280;

    // Featured commodity preview below the details card.
    constexpr Point<int16_t> FEATURED_PANEL_POS{ 508, 396 };
    constexpr Point<int16_t> FEATURED_PANEL_SIZE{ 288, 196 };
    constexpr Point<int16_t> FEATURED_BUTTON_POS{ 508, 396 };
    constexpr Point<int16_t> FEATURED_BUTTON_SIZE{ 288, 148 };
    constexpr Point<int16_t> FEATURED_TITLE_POS{ 516, 400 };
    constexpr Point<int16_t> FEATURED_HEADER_SIZE{ 288, 28 };
    constexpr Point<int16_t> FEATURED_HEADER_LINE_POS{ 508, 424 };
    constexpr Point<int16_t> FEATURED_HEADER_LINE_SIZE{ 288, 1 };
    constexpr Point<int16_t> FEATURED_BADGE_POS{ 762, 400 };
    constexpr Point<int16_t> FEATURED_BADGE_SIZE{ 26, 24 };
    constexpr Point<int16_t> FEATURED_AVATAR_FRAME_POS{ 516, 434 };
    constexpr Point<int16_t> FEATURED_AVATAR_FRAME_SIZE{ 92, 110 };
    constexpr Point<int16_t> FEATURED_AVATAR_POS{ 562, 540 };
    constexpr Point<int16_t> FEATURED_AVATAR_FLOOR_POS{ 530, 540 };
    constexpr Point<int16_t> FEATURED_AVATAR_FLOOR_SIZE{ 64, 2 };
    constexpr Point<int16_t> FEATURED_ICON_FRAME_POS{ 524, 446 };
    constexpr Point<int16_t> FEATURED_ICON_FRAME_SIZE{ 76, 76 };
    constexpr Point<int16_t> FEATURED_ICON_CENTER{ 562, 484 };
    constexpr int16_t FEATURED_ICON_MAX = 62;
    constexpr Point<int16_t> FEATURED_TEXT_BLOCK_POS{ 616, 434 };
    constexpr Point<int16_t> FEATURED_TEXT_BLOCK_SIZE{ 172, 84 };
    constexpr Point<int16_t> FEATURED_NAME_POS{ 624, 438 };
    constexpr Point<int16_t> FEATURED_PRICE_ICON_POS{ 624, 462 };
    constexpr Point<int16_t> FEATURED_META_POS{ 624, 486 };
    constexpr int16_t FEATURED_TEXT_WIDTH = 156;
    constexpr Point<int16_t> FEATURED_HINT_BAND_POS{ 508, 552 };
    constexpr Point<int16_t> FEATURED_HINT_BAND_SIZE{ 288, 40 };
    constexpr Point<int16_t> FEATURED_HINT_POS{ 678, 562 };
    constexpr int16_t FEATURED_HINT_WIDTH = 220;
    constexpr Point<int16_t> RESET_POS{ 516, 562 };
    constexpr Point<int16_t> RESET_SIZE{ 48, 18 };

    // Cursor x that separates the catalog from the right pane when scrolling.
    constexpr int16_t SCROLL_SPLIT_X = 504;

    // Viewport-space x of the catalog/right-pane scroll split.
    inline int16_t scroll_split_x()
    {
        return static_cast<int16_t>(origin().x() + scale_coord(SCROLL_SPLIT_X));
    }

    // Top-left corner of the n-th category entry.
    inline Point<int16_t> category_position(int8_t index)
    {
        return {
            CATEGORY_POS.x(),
            static_cast<int16_t>(CATEGORY_POS.y() + index * CATEGORY_STEP_Y)
        };
    }

    // Top-left corner of the n-th equipment sub-category entry.
    inline Point<int16_t> equip_sub_position(int8_t index)
    {
        return {
            EQSUB_POS.x(),
            static_cast<int16_t>(EQSUB_POS.y() +
                index * EQSUB_ENTRY_STEP_Y)
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
