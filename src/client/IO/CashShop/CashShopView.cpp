#include "CashShopView.h"

#include <algorithm>

namespace jrc::cash_shop_view
{
    namespace
    {
        bool gender_matches(int8_t gender, bool female)
        {
            return gender < 0 || gender == 2 || gender == (female ? 1 : 0);
        }

        bool query_matches(const CashCommodity& item, const std::string& query)
        {
            return query.empty() || item.name.find(query) != std::string::npos ||
                std::to_string(item.sn).find(query) != std::string::npos;
        }
    }

    std::vector<size_t> filter_catalog(
        const std::vector<CashCommodity>& catalog,
        const Filter& filter)
    {
        std::vector<size_t> filtered;
        for (size_t index = 0; index < catalog.size(); ++index)
        {
            const CashCommodity& item = catalog[index];
            const bool category_matches =
                filter.category == 0 || item.category == filter.category;
            const bool equip_matches =
                filter.category != EQUIP_CATEGORY || filter.equip_sub == 0 ||
                equip_sub_of(item.item_id) == filter.equip_sub;
            if (item.on_sale && gender_matches(item.gender, filter.female) &&
                category_matches && equip_matches &&
                query_matches(item, filter.query))
            {
                filtered.push_back(index);
            }
        }
        return filtered;
    }

    size_t page_count(size_t item_count, size_t page_size)
    {
        return std::max<size_t>(
            1, (item_count + page_size - 1) / page_size);
    }

    size_t clamp_page(size_t page, size_t item_count, size_t page_size)
    {
        return std::min(page, page_count(item_count, page_size) - 1);
    }

    std::pair<size_t, size_t> page_range(
        size_t page, size_t item_count, size_t page_size)
    {
        const size_t safe_page = clamp_page(page, item_count, page_size);
        const size_t begin = std::min(item_count, safe_page * page_size);
        return { begin, std::min(item_count, begin + page_size) };
    }

    std::optional<size_t> highlighted_index(size_t page,
        const std::vector<size_t>& filtered, std::optional<size_t> selected)
    {
        const auto range = page_range(page, filtered.size(), CATALOG_PAGE_SIZE);
        if (range.first >= range.second)
            return std::nullopt;
        if (selected && *selected >= range.first && *selected < range.second)
            return selected;
        return range.first;
    }

    int8_t next_category(int8_t category)
    {
        return static_cast<int8_t>((category + 1) % CATEGORY_COUNT);
    }

    CashCurrency next_currency(CashCurrency currency)
    {
        switch (currency)
        {
        case CashCurrency::NX_CREDIT: return CashCurrency::MAPLE_POINT;
        case CashCurrency::MAPLE_POINT: return CashCurrency::NX_PREPAID;
        case CashCurrency::NX_PREPAID: return CashCurrency::NX_CREDIT;
        }
        return CashCurrency::NX_CREDIT;
    }

    RightPane toggle_pane(RightPane pane)
    {
        return pane == RightPane::LOCKER ? RightPane::INVENTORY : RightPane::LOCKER;
    }

    CommodityBadge commodity_badge(const CashCommodity& item)
    {
        if (item.count > 1)
            return CommodityBadge::SALE;
        if (item.period > 0 && item.period <= 30)
            return CommodityBadge::LIMITED;
        if (item.package)
            return CommodityBadge::EVENT;
        if (item.priority >= 10)
            return CommodityBadge::NEW;
        if (item.priority >= 8)
            return CommodityBadge::HOT;
        return CommodityBadge::NONE;
    }

    const char* badge_name(CommodityBadge badge)
    {
        switch (badge)
        {
        case CommodityBadge::SALE: return "特惠";
        case CommodityBadge::LIMITED: return "限时";
        case CommodityBadge::EVENT: return "活动";
        case CommodityBadge::NEW: return "新品";
        case CommodityBadge::HOT: return "热卖";
        case CommodityBadge::NONE: return "";
        }
        return "";
    }

    const char* category_name(int8_t category)
    {
        constexpr const char* names[CATEGORY_COUNT] = {
            "全部", "装备", "消耗", "设置", "其他", "现金", "礼包"
        };
        return category >= 0 && category < CATEGORY_COUNT ? names[category] : names[0];
    }

    int8_t equip_sub_of(int32_t item_id)
    {
        if (item_id / 1'000'000 != 1)
            return 0;
        const int32_t slot = item_id / 10'000 - 100;
        if (slot < 0 || (slot > 14 && slot < 30))
            return 0;
        return slot >= 30 ? static_cast<int8_t>(EQSUB_COUNT - 1)
                          : static_cast<int8_t>(slot + 1);
    }

    const char* equip_sub_name(int8_t sub)
    {
        constexpr const char* names[EQSUB_COUNT] = {
            "全部装备", "帽子", "脸饰", "眼饰", "耳环", "上衣", "套装",
            "裤子", "鞋子", "手套", "盾牌", "披风", "戒指", "坠饰",
            "腰带", "勋章", "武器"
        };
        return sub >= 0 && sub < EQSUB_COUNT ? names[sub] : names[0];
    }

    const char* currency_name(CashCurrency currency)
    {
        switch (currency)
        {
        case CashCurrency::NX_CREDIT: return "NX Credit";
        case CashCurrency::MAPLE_POINT: return "Maple Point";
        case CashCurrency::NX_PREPAID: return "Prepaid NX";
        }
        return "NX Credit";
    }

    const char* pane_name(RightPane pane)
    {
        return pane == RightPane::LOCKER ? "商城仓库" : "角色现金栏";
    }

    const char* transfer_name(RightPane pane)
    {
        return pane == RightPane::LOCKER ? "[ 取出 ]" : "[ 存入 ]";
    }

    std::string format_amount(int32_t amount)
    {
        std::string text = std::to_string(amount);
        for (int32_t index = static_cast<int32_t>(text.size()) - 3;
             index > 0; index -= 3)
        {
            text.insert(static_cast<size_t>(index), ",");
        }
        return text;
    }
}
