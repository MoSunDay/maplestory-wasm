//////////////////////////////////////////////////////////////////////////////
// Texture bundle for the 2023 CashShop.img skin plus bitmap-number helpers.
//
// load_cash_shop_skin() hard-fails (std::runtime_error naming the node path)
// when any required bitmap or four-state button is missing or has an
// unexpected size, mirroring the client's other asset contracts.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <cstdint>

#include "../../Graphics/Color.h"
#include "../../Graphics/Texture.h"
#include "../../Template/Point.h"

namespace jrc::cash_shop_skin
{
    // Digits 0-9 plus the comma glyph at index 10.
    constexpr size_t NUMBER_GLYPHS = 11;
    constexpr size_t COMMA_GLYPH = 10;

    // Four frames in CashShop.img order: normal, pressed, mouseOver, disabled.
    struct CashShopButtonSkin
    {
        Texture frames[4];
    };

    struct CashShopSkin
    {
        Texture background;
        Texture preview[3];
        Texture number[NUMBER_GLYPHS];
        Texture no_item;
        Texture line;
        Texture sort_bar;
        Texture card;
        Texture inventory_panel;
        Texture inventory_cover;
        Texture tab_enabled;
        Texture tab_disabled;
        Texture tab_bar;
        Texture badge_new;
        Texture badge_hot;
        Texture badge_sale;
        Texture badge_event;
        Texture badge_limited;
        Texture currency_icon[2];
        Texture nx_icon;
        Texture selected_marker;
        Texture locker_cover;

        // Four-state action buttons, drawn stretched on the screen grid.
        CashShopButtonSkin buy_button;
        CashShopButtonSkin transfer_button;
        CashShopButtonSkin charge_button;
        CashShopButtonSkin exit_button;
        CashShopButtonSkin prev_button;
        CashShopButtonSkin next_button;
    };

    // Loads and validates every CashShop.img node the modern renderer uses.
    CashShopSkin load_cash_shop_skin();

    // Draws an amount with the bitmap number font, comma separated.
    void draw_amount(
        const CashShopSkin& skin, Point<int16_t> position, int32_t amount,
        Color color = Color::WHITE);

    // Width the amount would occupy in the bitmap number font.
    int16_t amount_width(const CashShopSkin& skin, int32_t amount);

    // Draws the amount right aligned so it ends at the given point.
    void draw_amount_right(
        const CashShopSkin& skin, Point<int16_t> right, int32_t amount,
        Color color = Color::WHITE);
}
