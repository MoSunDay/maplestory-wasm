#include <iostream>
#include <vector>

#include "IO/CashShop/CashShopView.h"

namespace
{
    jrc::CashCommodity item(int32_t sn, int8_t gender, int8_t category,
        bool on_sale, const char* name)
    {
        jrc::CashCommodity value;
        value.sn = sn;
        value.gender = gender;
        value.category = category;
        value.on_sale = on_sale;
        value.name = name;
        return value;
    }

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }
}

int main()
{
    using namespace jrc;
    using namespace cash_shop_view;
    const std::vector<CashCommodity> catalog = {
        item(10000001, 2, 1, true, "Shared Hat"),
        item(10000002, 0, 2, true, "Male Potion"),
        item(10000003, 1, 1, true, "Female Hat"),
        item(10000004, 2, 1, false, "Hidden Hat")
    };

    bool ok = true;
    ok &= expect(filter_catalog(catalog, {false, 0, ""}) ==
        std::vector<size_t>({0, 1}), "male and on-sale filtering");
    ok &= expect(filter_catalog(catalog, {true, 1, ""}) ==
        std::vector<size_t>({0, 2}), "female/category filtering");
    auto shared = item(10000005, -1, 1, true, "Server Shared");
    ok &= expect(filter_catalog({shared}, {false, 0, ""}) == std::vector<size_t>({0}),
        "linked-server shared gender filtering");
    ok &= expect(filter_catalog(catalog, {false, 0, "10000002"}) ==
        std::vector<size_t>({1}), "SN search");
    ok &= expect(filter_catalog(catalog, {true, 0, "Female"}) ==
        std::vector<size_t>({2}), "name search");
    ok &= expect(page_count(0) == 1 && page_count(5) == 1 && page_count(6) == 2,
        "page counts");
    ok &= expect(clamp_page(9, 6) == 1 && page_range(9, 6) ==
        std::pair<size_t, size_t>({5, 6}), "page clamping");
    ok &= expect(next_category(6) == 0, "category cycling");
    ok &= expect(next_currency(CashCurrency::NX_PREPAID) == CashCurrency::NX_CREDIT,
        "currency cycling");
    ok &= expect(toggle_pane(RightPane::LOCKER) == RightPane::INVENTORY,
        "right pane toggle");
    ok &= expect(format_amount(1234567) == "1,234,567", "amount formatting");
    if (ok)
        std::cout << "PASS cash shop view policy\n";
    return ok ? 0 : 1;
}
