#pragma once

#include "CashShopModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jrc::cash_shop_view
{
    constexpr size_t CATALOG_COLS = 4;
    constexpr size_t CATALOG_ROWS = 3;
    constexpr size_t CATALOG_PAGE_SIZE = CATALOG_COLS * CATALOG_ROWS;
    constexpr size_t RIGHT_COLS = 7;
    constexpr size_t RIGHT_ROWS = 3;
    constexpr size_t RIGHT_PAGE_SIZE = RIGHT_COLS * RIGHT_ROWS;
    constexpr int8_t CATEGORY_COUNT = 7;
    // Index of the 装备 entry in the category rail.
    constexpr int8_t EQUIP_CATEGORY = 1;
    // Equipment sub-categories opened from the rail's 装备 entry.
    constexpr int8_t EQSUB_COUNT = 17;

    // Equipment slots that the CharLook render stack actually draws. Rings,
    // pendants, belts and medals have no visible layer, so they stay out.
    constexpr bool is_previewable_equip(int32_t item_id)
    {
        if (item_id / 1000000 != 1)
            return false;
        const int32_t index = item_id / 10000 - 100;
        return index >= 0 && (index < 11 || (index >= 30 && index < 50));
    }

    enum class RightPane : uint8_t
    {
        LOCKER,
        INVENTORY
    };

    enum class CommodityBadge : uint8_t
    {
        NONE,
        SALE,
        LIMITED,
        EVENT,
        NEW,
        HOT
    };

    struct Filter
    {
        bool female = false;
        int8_t category = 0;
        std::string query;
        int8_t equip_sub = 0;
    };

    std::vector<size_t> filter_catalog(
        const std::vector<CashCommodity>& catalog,
        const Filter& filter
    );

    size_t page_count(size_t item_count, size_t page_size);
    size_t clamp_page(size_t page, size_t item_count, size_t page_size);
    std::pair<size_t, size_t> page_range(
        size_t page, size_t item_count, size_t page_size);
    std::optional<size_t> highlighted_index(size_t page,
        const std::vector<size_t>& filtered, std::optional<size_t> selected);

    int8_t next_category(int8_t category);
    int8_t equip_sub_of(int32_t item_id);
    const char* equip_sub_name(int8_t sub);
    CashCurrency next_currency(CashCurrency currency);
    RightPane toggle_pane(RightPane pane);

    const char* category_name(int8_t category);
    const char* currency_name(CashCurrency currency);
    const char* pane_name(RightPane pane);
    const char* transfer_name(RightPane pane);
    CommodityBadge commodity_badge(const CashCommodity& item);
    const char* badge_name(CommodityBadge badge);
    std::string format_amount(int32_t amount);
}
