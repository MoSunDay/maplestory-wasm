#pragma once

#include "CashShopModel.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace jrc::cash_shop_view
{
    constexpr size_t ROWS_PER_PANE = 5;
    constexpr int8_t CATEGORY_COUNT = 7;

    enum class RightPane : uint8_t
    {
        LOCKER,
        INVENTORY
    };

    struct Filter
    {
        bool female = false;
        int8_t category = 0;
        std::string query;
    };

    std::vector<size_t> filter_catalog(
        const std::vector<CashCommodity>& catalog,
        const Filter& filter
    );

    size_t page_count(size_t item_count);
    size_t clamp_page(size_t page, size_t item_count);
    std::pair<size_t, size_t> page_range(size_t page, size_t item_count);

    int8_t next_category(int8_t category);
    CashCurrency next_currency(CashCurrency currency);
    RightPane toggle_pane(RightPane pane);

    const char* category_name(int8_t category);
    const char* currency_name(CashCurrency currency);
    const char* pane_name(RightPane pane);
    const char* transfer_name(RightPane pane);
    std::string format_amount(int32_t amount);
}
