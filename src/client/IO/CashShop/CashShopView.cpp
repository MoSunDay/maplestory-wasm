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
            if (item.on_sale && gender_matches(item.gender, filter.female) &&
                category_matches && query_matches(item, filter.query))
            {
                filtered.push_back(index);
            }
        }
        return filtered;
    }

    size_t page_count(size_t item_count)
    {
        return std::max<size_t>(1, (item_count + ROWS_PER_PANE - 1) / ROWS_PER_PANE);
    }

    size_t clamp_page(size_t page, size_t item_count)
    {
        return std::min(page, page_count(item_count) - 1);
    }

    std::pair<size_t, size_t> page_range(size_t page, size_t item_count)
    {
        const size_t safe_page = clamp_page(page, item_count);
        const size_t begin = std::min(item_count, safe_page * ROWS_PER_PANE);
        return { begin, std::min(item_count, begin + ROWS_PER_PANE) };
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

    const char* category_name(int8_t category)
    {
        constexpr const char* names[CATEGORY_COUNT] = {
            "全部", "装备", "消耗", "设置", "其他", "现金", "礼包"
        };
        return category >= 0 && category < CATEGORY_COUNT ? names[category] : names[0];
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
