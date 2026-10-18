//////////////////////////////////////////////////////////////////////////////
// Design-space geometry for the modern full-screen cash shop renderer.
//
// The cash shop paints the 2023 CashShop.img set inside a fixed 800x600
// design space. The browser viewport matches the design size, so the scale
// is 1:1 there; desktop windows of any other size letterbox the whole shop
// centered with a uniform scale. Every element position below is authored
// in design units and converted with to_screen() at draw/input time.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace jrc::cash_shop_layout
{
    constexpr int16_t DESIGN_WIDTH = 800;
    constexpr int16_t DESIGN_HEIGHT = 600;

    // Title and the top-right balance block.
    constexpr Point<int16_t> TITLE_POS{ 12, 8 };
    constexpr Point<int16_t> CHARGE_POS{ 713, 12 };
    constexpr int16_t BALANCE_LABEL_X = 600;
    constexpr int16_t BALANCE_VALUE_RIGHT_X = 708;
    constexpr int16_t BALANCE_ROWS_Y[3]{ 6, 20, 34 };

    // Search bar (CSSortBar stretched) with its text field and hit area.
    constexpr Point<int16_t> SEARCH_BAR_POS{ 112, 56 };
    constexpr Point<int16_t> SEARCH_BAR_SIZE{ 378, 26 };
    constexpr Point<int16_t> SEARCH_LABEL_POS{ 122, 62 };
    constexpr Rectangle<int16_t> SEARCH_FIELD_BOUNDS{
        { 156, 60 }, { 370, 78 } };
    constexpr Rectangle<int16_t> SEARCH_HITBOX{
        { 112, 56 }, { 490, 82 } };

    // Payment currency chips under the search bar.
    constexpr Point<int16_t> CURRENCY_POS{ 118, 86 };
    constexpr Point<int16_t> CURRENCY_SIZE{ 62, 16 };
    constexpr int16_t CURRENCY_STEP_X = 66;

    // Left category column painted on the background's dark strip.
    constexpr Point<int16_t> CATEGORY_POS{ 6, 48 };
    constexpr Point<int16_t> CATEGORY_SIZE{ 88, 26 };
    constexpr int16_t CATEGORY_STEP_Y = 34;
    constexpr Point<int16_t> CATEGORY_LABEL_OFFSET{ 0, 6 };
    constexpr Point<int16_t> CATEGORY_LINE_OFFSET{ 0, 16 };
    constexpr Point<int16_t> CATEGORY_LINE_SIZE{ 88, 2 };

    // Commodity card grid (3x2 cards of the native 119x184 CSList/Base).
    constexpr Point<int16_t> GRID_POS{ 118, 96 };
    constexpr int16_t GRID_STEP_X = 128;
    constexpr int16_t GRID_STEP_Y = 196;
    constexpr Point<int16_t> CARD_SIZE{ 119, 184 };
    constexpr Point<int16_t> CARD_ICON_CENTER{ 59, 28 };
    constexpr Point<int16_t> CARD_NAME_POS{ 59, 114 };
    constexpr int16_t CARD_NAME_WIDTH = 112;
    constexpr Point<int16_t> CARD_PRICE_ICON_POS{ 30, 148 };
    constexpr Point<int16_t> CARD_MARKER_OFFSET{ 2, 2 };
    constexpr Point<int16_t> CARD_BUY_OFFSET{ 9, 58 };
    constexpr Point<int16_t> EMPTY_HINT_SIZE{ 240, 102 };
    constexpr Point<int16_t> EMPTY_CATALOG_CENTER{ 305, 288 };

    // Left pager.
    constexpr Point<int16_t> LEFT_PREV_POS{ 128, 470 };
    constexpr Point<int16_t> LEFT_NEXT_POS{ 452, 470 };
    constexpr Point<int16_t> LEFT_PAGE_POS{ 280, 488 };
    constexpr int16_t LEFT_PAGE_WIDTH = 60;

    // Right pane mode chips, inventory panel and slot grid.
    constexpr Point<int16_t> TAB_LOCKER_POS{ 506, 56 };
    constexpr Point<int16_t> TAB_INVENTORY_POS{ 582, 56 };
    constexpr Point<int16_t> TAB_SIZE{ 70, 18 };
    constexpr int16_t TAB_LABEL_WIDTH = 68;
    constexpr Point<int16_t> PANEL_POS{ 500, 78 };
    constexpr Point<int16_t> PANEL_SIZE{ 298, 168 };
    constexpr Point<int16_t> SLOT_GRID_POS{ 508, 132 };
    constexpr int16_t SLOT_STEP = 36;
    constexpr Point<int16_t> SLOT_SIZE{ 36, 36 };
    constexpr Point<int16_t> SLOT_COUNT_OFFSET{ 34, 22 };
    constexpr Point<int16_t> SLOT_MARKER_OFFSET{ 1, 1 };
    constexpr Point<int16_t> TRANSFER_POS{ 612, 252 };
    constexpr Point<int16_t> TRANSFER_HINT_POS{ 506, 254 };

    // Right pager and exit button.
    constexpr Point<int16_t> RIGHT_PREV_POS{ 560, 380 };
    constexpr Point<int16_t> RIGHT_NEXT_POS{ 740, 380 };
    constexpr Point<int16_t> RIGHT_PAGE_POS{ 620, 398 };
    constexpr int16_t RIGHT_PAGE_WIDTH = 120;
    constexpr Point<int16_t> EXIT_POS{ 2, 560 };

    // Uniform scale that keeps the whole 800x600 design space visible.
    inline float content_scale(int16_t width, int16_t height)
    {
        const float horizontal = static_cast<float>(width) / DESIGN_WIDTH;
        const float vertical = static_cast<float>(height) / DESIGN_HEIGHT;
        return std::min(horizontal, vertical);
    }

    // Letterbox offset that centers the scaled design space.
    inline Point<int16_t> content_offset(int16_t width, int16_t height)
    {
        const float scale = content_scale(width, height);
        return {
            static_cast<int16_t>((width - DESIGN_WIDTH * scale) / 2),
            static_cast<int16_t>((height - DESIGN_HEIGHT * scale) / 2)
        };
    }

    // Map a design-space point onto the screen.
    inline Point<int16_t> to_screen(
        Point<int16_t> design, float scale, Point<int16_t> offset)
    {
        return {
            static_cast<int16_t>(offset.x() + design.x() * scale),
            static_cast<int16_t>(offset.y() + design.y() * scale)
        };
    }

    // Map a screen point back into design space (for hit testing).
    inline Point<int16_t> to_design(
        Point<int16_t> screen, float scale, Point<int16_t> offset)
    {
        if (scale <= 0.0f)
            return screen;
        return {
            static_cast<int16_t>((screen.x() - offset.x()) / scale),
            static_cast<int16_t>((screen.y() - offset.y()) / scale)
        };
    }

    // Scale a design-space size, keeping degenerate results drawable.
    inline Point<int16_t> to_screen_size(Point<int16_t> size, float scale)
    {
        return {
            std::max<int16_t>(1, static_cast<int16_t>(size.x() * scale)),
            std::max<int16_t>(1, static_cast<int16_t>(size.y() * scale))
        };
    }

    // Top-left corner of the n-th category entry.
    inline Point<int16_t> category_position(int8_t index)
    {
        return {
            CATEGORY_POS.x(),
            static_cast<int16_t>(CATEGORY_POS.y() + index * CATEGORY_STEP_Y)
        };
    }

    // Top-left corner of the n-th catalog card on a page.
    inline Point<int16_t> card_position(size_t index)
    {
        return {
            static_cast<int16_t>(GRID_POS.x() + (index % 3) * GRID_STEP_X),
            static_cast<int16_t>(GRID_POS.y() + (index / 3) * GRID_STEP_Y)
        };
    }

    // Top-left corner of the n-th inventory slot on a page.
    inline Point<int16_t> slot_position(size_t index)
    {
        return {
            static_cast<int16_t>(SLOT_GRID_POS.x() + (index % 8) * SLOT_STEP),
            static_cast<int16_t>(SLOT_GRID_POS.y() + (index / 8) * SLOT_STEP)
        };
    }

    // Center of the right pane panel, used by its empty-page placeholder.
    constexpr Point<int16_t> EMPTY_RIGHT_CENTER{
        PANEL_POS.x() + PANEL_SIZE.x() / 2,
        PANEL_POS.y() + PANEL_SIZE.y() / 2
    };
}
