#pragma once

#include "CashShopView.h"
#include "../Components/Button.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace jrc
{
    // Shared hit-area geometry for the cash shop buttons. The design-space
    // position and size of every button id live here so the constructor and
    // update_screen() can (re)apply them on the uniformly scaled canvas.
    namespace cash_shop_buttons
    {
        constexpr size_t CURRENCY_CHOICES = 3;

        // Creates missing AreaButtons and re-anchors existing ones onto the
        // scaled 800x600 design canvas.
        void apply_geometry(
            std::map<uint16_t, std::unique_ptr<Button>>& buttons);
    }

    // Button ids shared by UICashShop's draw and input code.
    enum Buttons : uint16_t
    {
        BT_EXIT,
        BT_CHARGE,
        BT_BUY,
        BT_RESET,
        BT_EQSUB_CLOSE,
        BT_EQSUB_FIRST,
        BT_EQSUB_LAST = BT_EQSUB_FIRST + cash_shop_view::EQSUB_COUNT,
        BT_CARD_FIRST,
        BT_CARD_LAST = BT_CARD_FIRST + cash_shop_view::CATALOG_PAGE_SIZE,
        BT_SLOT_FIRST,
        BT_SLOT_LAST = BT_SLOT_FIRST + cash_shop_view::RIGHT_PAGE_SIZE,
        BT_TAB_FIRST,
        BT_TAB_LAST = BT_TAB_FIRST + cash_shop_view::CATEGORY_COUNT,
        BT_CURRENCY_FIRST,
        BT_CURRENCY_LAST =
            BT_CURRENCY_FIRST + cash_shop_buttons::CURRENCY_CHOICES,
        BT_RIGHT_LOCKER_TAB,
        BT_RIGHT_INVENTORY_TAB,
        BT_TRANSFER,
        BT_LEFT_PREV,
        BT_LEFT_NEXT,
        BT_RIGHT_PREV,
        BT_RIGHT_NEXT,
        BT_FEATURED
    };
}
