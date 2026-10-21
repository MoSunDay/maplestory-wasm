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
    // Title, three currency readouts and the two action pills.
    constexpr Point<int16_t> TITLE_POS{ 109, 3 };
    constexpr Point<int16_t> TOP_BAR_POS{ 100, 0 };
    constexpr Point<int16_t> TOP_BAR_SIZE{ 700, 32 };
    constexpr int16_t BALANCE_ICON_XS[3]{ 184, 294, 404 };
    constexpr int16_t BALANCE_ICON_Y = 6;
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
    constexpr Point<int16_t> CATEGORY_COUNT_OFFSET{ 84, 1 };
    constexpr int16_t CATEGORY_COUNT_WIDTH = 40;
    constexpr Point<int16_t> CATEGORY_LINE_OFFSET{ 0, 21 };
    constexpr Point<int16_t> CATEGORY_LINE_SIZE{ 90, 2 };

    // --- Equipment sub-menu ----------------------------------------------
    // Flyout opened from the rail's 装备 entry. It overlays the first
    // catalog column while an equipment sub-category is being picked.
    constexpr Point<int16_t> EQSUB_POS{ 100, 86 };
    constexpr int16_t EQSUB_WIDTH = 96;
    constexpr int16_t EQSUB_HEADER_HEIGHT = 20;
    constexpr int16_t EQSUB_ENTRY_STEP_Y = 22;
    constexpr Point<int16_t> EQSUB_ENTRY_SIZE{ EQSUB_WIDTH, EQSUB_ENTRY_STEP_Y };
    constexpr Point<int16_t> EQSUB_HEADER_LABEL_OFFSET{ 8, 0 };
    constexpr Point<int16_t> EQSUB_LABEL_OFFSET{ 8, 1 };
    constexpr Point<int16_t> EQSUB_COUNT_OFFSET{ 54, 1 };
    constexpr int16_t EQSUB_COUNT_WIDTH = 36;
    constexpr Point<int16_t> EQSUB_CLOSE_POS{ 178, 88 };
    constexpr Point<int16_t> EQSUB_CLOSE_SIZE{ 14, 16 };

    // Legend explaining the badge overlays on cards and slots.
    constexpr Point<int16_t> LEGEND_PANEL_POS{ 4, 274 };
    constexpr Point<int16_t> LEGEND_PANEL_SIZE{ 90, 172 };
    constexpr Point<int16_t> LEGEND_TITLE_POS{ 12, 280 };
    constexpr Point<int16_t> LEGEND_ROW_POS{ 10, 300 };
    constexpr int16_t LEGEND_ROW_STEP = 29;
    constexpr Point<int16_t> LEGEND_BADGE_SIZE{ 24, 22 };
    constexpr Point<int16_t> LEGEND_LABEL_OFFSET{ 38, 4 };
    constexpr int16_t LEGEND_LABEL_WIDTH = 48;
    constexpr Point<int16_t> LEGEND_NOTE_POS{ 8, 452 };
    constexpr int16_t LEGEND_NOTE_WIDTH = 82;

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
    constexpr Point<int16_t> CURRENCY_POS{ 109, 66 };
    constexpr Point<int16_t> CURRENCY_SIZE{ 48, 16 };
    constexpr int16_t CURRENCY_STEP_X = 52;

    // Filter summary shown above the right pane.
    constexpr Point<int16_t> CATALOG_INFO_POS{ 492, 69 };
    constexpr int16_t CATALOG_INFO_WIDTH = 224;

    // Native card grid at (140,110) with 119x184 cards and a 124/189 pitch.
    constexpr Point<int16_t> GRID_POS{ 109, 86 };
    constexpr int16_t GRID_STEP_X = 97;
    constexpr int16_t GRID_STEP_Y = 148;
    constexpr Point<int16_t> CARD_SIZE{ 93, 144 };
    constexpr Point<int16_t> CARD_ICON_CENTER{ 46, 34 };
    constexpr int16_t CARD_ICON_MAX = 56;
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

    // Footer rule and hint under the catalog grid.
    constexpr Point<int16_t> CATALOG_LINE_POS{ 109, 573 };
    constexpr Point<int16_t> CATALOG_LINE_SIZE{ 384, 2 };
    constexpr Point<int16_t> CATALOG_HINT_POS{ 301, 578 };
    constexpr int16_t CATALOG_HINT_WIDTH = 384;

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
    // Canvas size of the 346x146 slot-grid cover bitmap.
    constexpr Point<int16_t> SLOT_COVER_SIZE{ px(346), px(146) };
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
    constexpr Point<int16_t> DETAILS_TEXT_CENTER{ 700, 310 };
    constexpr int16_t DETAILS_TEXT_WIDTH = 170;
    constexpr int16_t DETAILS_COUNT_Y = 338;
    constexpr int16_t DETAILS_META_Y = 356;
    constexpr int16_t DETAILS_EXTRA_Y = 376;
    constexpr Point<int16_t> DETAILS_HINT_CENTER{ 644, 340 };
    constexpr int16_t DETAILS_HINT_WIDTH = 280;

    // Featured commodity panel below the right pane.
    constexpr Point<int16_t> FEATURED_PANEL_POS{ 496, 426 };
    constexpr Point<int16_t> FEATURED_PANEL_SIZE{ 297, 162 };
    constexpr Point<int16_t> FEATURED_AVATAR_FRAME_POS{ 504, 452 };
    constexpr Point<int16_t> FEATURED_AVATAR_FRAME_SIZE{ 92, 98 };
    constexpr Point<int16_t> FEATURED_AVATAR_POS{ 550, 548 };
    constexpr Point<int16_t> FEATURED_AVATAR_FLOOR_POS{ 518, 548 };
    constexpr Point<int16_t> FEATURED_AVATAR_FLOOR_SIZE{ 64, 2 };
    constexpr Point<int16_t> FEATURED_TITLE_POS{ 508, 432 };
    constexpr Point<int16_t> FEATURED_BADGE_POS{ 755, 430 };
    constexpr Point<int16_t> FEATURED_BADGE_SIZE{ 26, 24 };
    constexpr Point<int16_t> FEATURED_ICON_FRAME_POS{ 512, 464 };
    constexpr Point<int16_t> FEATURED_ICON_FRAME_SIZE{ 76, 76 };
    constexpr Point<int16_t> FEATURED_ICON_CENTER{ 550, 502 };
    constexpr int16_t FEATURED_ICON_MAX = 62;
    constexpr Point<int16_t> FEATURED_NAME_POS{ 630, 454 };
    constexpr Point<int16_t> FEATURED_PRICE_ICON_POS{ 630, 486 };
    constexpr Point<int16_t> FEATURED_META_POS{ 630, 508 };
    constexpr int16_t FEATURED_TEXT_WIDTH = 156;
    constexpr Point<int16_t> FEATURED_HINT_POS{ 644, 568 };

    // Reset button for the accumulated try-on set, left of the hint band.
    constexpr Point<int16_t> RESET_POS{ 508, 556 };
    constexpr Point<int16_t> RESET_SIZE{ 40, 16 };
    constexpr int16_t FEATURED_HINT_WIDTH = 270;
    constexpr Point<int16_t> FEATURED_TEXT_BLOCK_POS{ 622, 448 };
    constexpr Point<int16_t> FEATURED_TEXT_BLOCK_SIZE{ 164, 74 };

    // Cursor x that separates the catalog from the right pane when scrolling.
    constexpr int16_t SCROLL_SPLIT_X = 495;

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

    // Size of the whole equipment flyout: header plus every sub entry.
    inline Point<int16_t> equip_menu_size()
    {
        return {
            EQSUB_WIDTH,
            static_cast<int16_t>(EQSUB_HEADER_HEIGHT +
                cash_shop_view::EQSUB_COUNT * EQSUB_ENTRY_STEP_Y)
        };
    }

    // Top-left corner of the n-th equipment sub-category entry.
    inline Point<int16_t> equip_sub_position(int8_t index)
    {
        return {
            EQSUB_POS.x(),
            static_cast<int16_t>(EQSUB_POS.y() + EQSUB_HEADER_HEIGHT +
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
