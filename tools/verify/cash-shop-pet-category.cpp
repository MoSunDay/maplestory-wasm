// 商城宠物分类策略验证：
//   1) 分类栏第 8 格（宠物）与共享左栏图例的布局边界；
//   2) category_for_item 行为表（宠物 5000xxx / 宠物装备 1812000-1812007 / 前缀归类）；
//   3) 8 个分类名；
//   4) filter_catalog 在售/性别/分类/搜索组合筛选；
//   5) commodity_badge 徽标优先级；
//   6) 34 只在售宠物的分页 sanity。
//
// c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Isrc/client -Isrc tools/verify/cash-shop-pet-category.cpp src/client/IO/CashShop/CashShopView.cpp -o /tmp/cash-shop-pet-category && /tmp/cash-shop-pet-category

#include "../../src/client/IO/CashShop/CashShopLayout.h"
#include "../../src/client/IO/CashShop/CashShopModel.h"
#include "../../src/client/IO/CashShop/CashShopView.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    int failures = 0;

    void expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL " << message << '\n';
        }
    }

    // Catalog entry whose rail category comes from the real policy, so the
    // filter tests exercise category_for_item end to end.
    jrc::CashCommodity catalog_item(int32_t item_id, int8_t gender,
        bool on_sale, const char* name)
    {
        jrc::CashCommodity value;
        value.sn = item_id;
        value.item_id = item_id;
        value.gender = gender;
        value.category = jrc::cash_shop_view::category_for_item(item_id);
        value.on_sale = on_sale;
        value.name = name;
        return value;
    }

    // Badge candidate filling only the fields commodity_badge reads.
    jrc::CashCommodity badge_item(int16_t count, int32_t period, bool package,
        int32_t priority)
    {
        jrc::CashCommodity value;
        value.count = count;
        value.period = period;
        value.package = package;
        value.priority = priority;
        return value;
    }

    bool pet_filter_is(const std::vector<jrc::CashCommodity>& catalog,
        const jrc::cash_shop_view::Filter& filter,
        const std::vector<size_t>& expected)
    {
        return jrc::cash_shop_view::filter_catalog(catalog, filter) ==
            expected;
    }
}

int main()
{
    using namespace jrc;
    using namespace jrc::cash_shop_view;
    namespace layout = jrc::cash_shop_layout;

    // ---- 1. Layout: the pet row and the legend share one 90px-wide rail.
    expect(CATEGORY_COUNT == 8 && PET_CATEGORY == 7 && EQUIP_CATEGORY == 1,
        "category rail constants (8 entries, equip=1, pet=7)");
    expect(layout::CATEGORY_STEP_Y == 22 && layout::CATEGORY_SIZE.x() == 90 &&
            layout::CATEGORY_SIZE.y() == 22,
        "category rail metrics (90x22 cells, 22px pitch)");
    const Point<int16_t> pet_pos = layout::category_position(PET_CATEGORY);
    expect(pet_pos.x() == layout::CATEGORY_POS.x() &&
            pet_pos.y() ==
                layout::CATEGORY_POS.y() +
                PET_CATEGORY * layout::CATEGORY_STEP_Y,
        "pet rail entry sits at y = 58 + 7*22");
    const int pet_bottom = pet_pos.y() + layout::CATEGORY_SIZE.y();
    expect(pet_bottom == 234 &&
            pet_bottom <= layout::CATEGORY_SECTION_LINE_POS.y(),
        "pet rail bottom 234 stays above the legend separator at 241");
    const int rail_bottom =
        layout::RAIL_PANEL_POS.y() + layout::RAIL_PANEL_SIZE.y();
    expect(rail_bottom == 546 && rail_bottom < layout::VIEW_HEIGHT,
        "single rail bottom 546 fits the 600-high view");
    expect(layout::LEGEND_NOTE_SEPARATOR_POS.y() >=
                layout::CATEGORY_SECTION_LINE_POS.y() &&
            layout::LEGEND_NOTE_SEPARATOR_POS.y() +
                    layout::LEGEND_NOTE_SEPARATOR_SIZE.y() <= rail_bottom,
        "legend separator sits inside the rail");
    expect(layout::LEGEND_NOTE_POS.y() >=
                layout::LEGEND_NOTE_SEPARATOR_POS.y() &&
            layout::RAIL_HELP_POS.y() + 30 <= rail_bottom,
        "legend note and help stay inside the rail");

    // ---- 2. category_for_item behaviour table.
    const std::vector<std::pair<int32_t, int8_t>> category_cases = {
        {5000000, PET_CATEGORY},
        {5000001, PET_CATEGORY},
        {5000099, PET_CATEGORY},
        {4999999, 4},
        {5001000, 5},
        {1812000, PET_CATEGORY},
        {1812007, PET_CATEGORY},
        {1812008, EQUIP_CATEGORY},
        {1812099, EQUIP_CATEGORY},
        {1002000, EQUIP_CATEGORY},
        {2000000, 2},
        {3000000, 3},
        {4000000, 4},
        {5010000, 5},
        {6000000, 6},
        {0, 6},
    };
    for (const auto& entry : category_cases)
        expect(category_for_item(entry.first) == entry.second,
            "category_for_item(" + std::to_string(entry.first) + ")");

    // ---- 3. Category rail labels.
    const std::vector<std::string> rail_names = {
        "全部", "装备", "消耗", "设置", "其他", "现金", "礼包", "宠物"
    };
    expect(rail_names.size() == static_cast<size_t>(CATEGORY_COUNT),
        "exactly eight category names");
    for (int8_t category = 0; category < CATEGORY_COUNT; ++category)
        expect(std::string(category_name(category)) ==
                rail_names[static_cast<size_t>(category)],
            "category_name(" + std::to_string(category) + ")");
    expect(std::string(category_name(PET_CATEGORY)) == "宠物",
        "pet category is labelled 宠物");

    // ---- 4. filter_catalog over a mixed catalog.
    const std::vector<CashCommodity> catalog = {
        catalog_item(5000001, 2, true, "Pet Meso"),
        catalog_item(1812000, 2, true, "Pet Collar"),
        catalog_item(5010000, 2, true, "Cash Effect"),
        catalog_item(1002000, 2, true, "Brown Cap"),
        catalog_item(5000002, 2, false, "Pet Hidden"),
        catalog_item(5000003, 1, true, "Pet Ribbon"),
    };
    expect(pet_filter_is(catalog, {false, PET_CATEGORY, "", 0},
            {0, 1}),
        "pet filter keeps on-sale pets and pet equipment");
    expect(pet_filter_is(catalog, {false, 0, "", 0}, {0, 1, 2, 3}),
        "all filter drops off-sale and gender-mismatched items");
    expect(pet_filter_is(catalog, {false, 5, "", 0}, {2}),
        "cash filter keeps only cash items, no pet leaks in");
    expect(pet_filter_is(catalog, {false, 0, "5000001", 0}, {0}),
        "SN search hits the pet");
    expect(pet_filter_is(catalog, {false, PET_CATEGORY, "Collar", 0}, {1}),
        "name search hits the pet equipment");
    expect(pet_filter_is(catalog, {true, PET_CATEGORY, "", 0}, {0, 1, 5}),
        "female filter adds the female-only pet");

    // ---- 5. commodity_badge precedence.
    expect(commodity_badge(badge_item(1, 90, false, 0)) ==
            CommodityBadge::NONE,
        "90-day period is not limited");
    expect(commodity_badge(badge_item(1, 30, false, 0)) ==
            CommodityBadge::LIMITED,
        "30-day period is limited");
    expect(commodity_badge(badge_item(1, 0, false, 8)) ==
            CommodityBadge::HOT,
        "priority 8 is hot");
    expect(commodity_badge(badge_item(1, 0, false, 11)) ==
            CommodityBadge::NEW,
        "priority 11 is new");
    expect(std::string(badge_name(CommodityBadge::LIMITED)) == "限时",
        "limited badge label");

    // ---- 6. Paging sanity with 34 on-sale pets.
    std::vector<CashCommodity> pets;
    for (int32_t item_id = 5000000; item_id <= 5000033; ++item_id)
        pets.push_back(catalog_item(item_id, 2, true, "Pet Bulk"));
    const std::vector<size_t> pet_catalog =
        filter_catalog(pets, {false, PET_CATEGORY, "", 0});
    expect(pet_catalog.size() == 34, "34 on-sale pets pass the pet filter");
    expect(page_count(pet_catalog.size(), CATALOG_PAGE_SIZE) == 3,
        "34 pets span three 12-card pages");
    expect(clamp_page(3, pet_catalog.size(), CATALOG_PAGE_SIZE) == 2 &&
            page_range(2, pet_catalog.size(), CATALOG_PAGE_SIZE) ==
                std::pair<size_t, size_t>({24, 34}),
        "last pet page covers items 24..33");

    if (failures == 0)
    {
        std::cout << "PASS cash shop pet category policy\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
