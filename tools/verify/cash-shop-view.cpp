#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "IO/CashShop/CashShopView.h"

namespace
{
    jrc::CashCommodity item(int32_t sn, int8_t gender, int8_t category,
        bool on_sale, const char* name, int16_t count = 1, int32_t period = 0,
        bool package = false, int32_t priority = 0)
    {
        jrc::CashCommodity value;
        value.sn = sn;
        value.gender = gender;
        value.category = category;
        value.on_sale = on_sale;
        value.name = name;
        value.count = count;
        value.period = period;
        value.package = package;
        value.priority = priority;
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
    ok &= expect(
        page_count(0, CATALOG_PAGE_SIZE) == 1 &&
        page_count(11, CATALOG_PAGE_SIZE) == 1 &&
        page_count(12, CATALOG_PAGE_SIZE) == 1 &&
        page_count(13, CATALOG_PAGE_SIZE) == 2,
        "catalog page counts");
    ok &= expect(
        clamp_page(9, 13, CATALOG_PAGE_SIZE) == 1 &&
        page_range(9, 13, CATALOG_PAGE_SIZE) ==
            std::pair<size_t, size_t>({12, 13}) &&
        page_range(0, 13, CATALOG_PAGE_SIZE) ==
            std::pair<size_t, size_t>({0, 12}),
        "catalog page clamping");
    ok &= expect(
        page_count(0, RIGHT_PAGE_SIZE) == 1 &&
        page_count(21, RIGHT_PAGE_SIZE) == 1 &&
        page_count(22, RIGHT_PAGE_SIZE) == 2 &&
        clamp_page(9, 22, RIGHT_PAGE_SIZE) == 1 &&
        page_range(9, 22, RIGHT_PAGE_SIZE) ==
            std::pair<size_t, size_t>({21, 22}) &&
        page_range(0, 50, RIGHT_PAGE_SIZE) ==
            std::pair<size_t, size_t>({0, 21}),
        "right pane paging");
    ok &= expect(next_category(6) == 0, "category cycling");
    ok &= expect(next_currency(CashCurrency::NX_PREPAID) == CashCurrency::NX_CREDIT,
        "currency cycling");
    ok &= expect(toggle_pane(RightPane::LOCKER) == RightPane::INVENTORY,
        "right pane toggle");
    ok &= expect(format_amount(1234567) == "1,234,567", "amount formatting");
    const CashCommodity sale = item(20000001, 2, 1, true, "Sale", 2, 30, true, 10);
    const CashCommodity limited = item(20000002, 2, 1, true, "Limited", 1, 30);
    const CashCommodity event = item(20000003, 2, 1, true, "Event", 1, 0, true, 10);
    const CashCommodity fresh = item(20000004, 2, 1, true, "New", 1, 31, false, 10);
    const CashCommodity hot = item(20000005, 2, 1, true, "Hot", 1, 0, false, 8);
    const CashCommodity plain = item(20000006, 2, 1, true, "Plain");
    ok &= expect(
        commodity_badge(sale) == CommodityBadge::SALE &&
        commodity_badge(limited) == CommodityBadge::LIMITED &&
        commodity_badge(event) == CommodityBadge::EVENT &&
        commodity_badge(fresh) == CommodityBadge::NEW &&
        commodity_badge(hot) == CommodityBadge::HOT &&
        commodity_badge(plain) == CommodityBadge::NONE,
        "commodity badge precedence");
    ok &= expect(
        commodity_badge(item(20000007, 2, 1, true, "Almost Hot", 1, 0, false, 7)) ==
            CommodityBadge::NONE &&
        commodity_badge(item(20000008, 2, 1, true, "Old", 1, 31)) ==
            CommodityBadge::NONE,
        "commodity badge thresholds");
    ok &= expect(
        std::string(badge_name(CommodityBadge::SALE)) == "特惠" &&
        std::string(badge_name(CommodityBadge::LIMITED)) == "限时" &&
        std::string(badge_name(CommodityBadge::EVENT)) == "活动" &&
        std::string(badge_name(CommodityBadge::NEW)) == "新品" &&
        std::string(badge_name(CommodityBadge::HOT)) == "热卖" &&
        std::string(badge_name(CommodityBadge::NONE)) == "",
        "commodity badge labels");
    ok &= expect(
        is_previewable_equip(1002000) && is_previewable_equip(1012180) &&
        is_previewable_equip(1022104) && is_previewable_equip(1042018) &&
        is_previewable_equip(1050050) && is_previewable_equip(1102000) &&
        is_previewable_equip(1302000) && is_previewable_equip(1492000),
        "visible equipment preview ranges");
    ok &= expect(
        !is_previewable_equip(1112000) && !is_previewable_equip(1122000) &&
        !is_previewable_equip(1702000) && !is_previewable_equip(5150000) &&
        !is_previewable_equip(4031192) && !is_previewable_equip(1202000),
        "non-previewable item ranges");
    // CATALOG_PAGE_SIZE is 12, so page 1 starts at filtered index 12.
    const std::vector<size_t> positions = {
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24
    };
    ok &= expect(
        highlighted_index(0, positions, std::nullopt) == size_t{0} &&
        highlighted_index(1, positions, std::nullopt) == size_t{12} &&
        highlighted_index(0, positions, size_t{1}) == size_t{1} &&
        highlighted_index(1, positions, size_t{0}) == size_t{12} &&
        highlighted_index(1, positions, size_t{14}) == size_t{14} &&
        !highlighted_index(0, {}, std::nullopt),
        "preview highlight selection");
    // Equipment sub-category flyout opened from the rail's 装备 entry.
    const auto equip = [](int32_t item_id) -> CashCommodity
    {
        CashCommodity value;
        value.sn = item_id;
        value.item_id = item_id;
        value.gender = 2;
        value.category = EQUIP_CATEGORY;
        value.on_sale = true;
        value.name = "Equip";
        return value;
    };
    const std::vector<CashCommodity> equips = {
        equip(1002000), equip(1012100), equip(1040016), equip(1072000),
        equip(1102000), equip(1112000), equip(1302000), equip(1492000),
        equip(1702000)
    };
    ok &= expect(
        equip_sub_of(1002000) == 1 && equip_sub_of(1012100) == 2 &&
        equip_sub_of(1040016) == 5 && equip_sub_of(1072000) == 8 &&
        equip_sub_of(1102000) == 11 && equip_sub_of(1112000) == 12 &&
        equip_sub_of(1302000) == 16 && equip_sub_of(1492000) == 16 &&
        equip_sub_of(1702000) == 16 && equip_sub_of(2000000) == 0 &&
        equip_sub_of(1150000) == 0,
        "equipment slot mapping");
    ok &= expect(
        std::string(equip_sub_name(0)) == "全部装备" &&
        std::string(equip_sub_name(1)) == "帽子" &&
        std::string(equip_sub_name(16)) == "武器" &&
        std::string(equip_sub_name(-1)) == "全部装备" &&
        std::string(equip_sub_name(EQSUB_COUNT)) == "全部装备",
        "equipment sub-category labels");
    ok &= expect(
        filter_catalog(equips, {false, EQUIP_CATEGORY, "", 0}).size() == 9 &&
        filter_catalog(equips, {false, EQUIP_CATEGORY, "", 1}) ==
            std::vector<size_t>({0}) &&
        filter_catalog(equips, {false, EQUIP_CATEGORY, "", 5}) ==
            std::vector<size_t>({2}) &&
        filter_catalog(equips, {false, EQUIP_CATEGORY, "", 16}) ==
            std::vector<size_t>({6, 7, 8}),
        "equipment sub-category filtering");
    ok &= expect(
        filter_catalog(equips, {false, 0, ""}).size() == 9 &&
        filter_catalog(equips, {false, EQUIP_CATEGORY, "", 1}).size() == 1,
        "equip sub ignored outside equipment category");
    if (ok)
        std::cout << "PASS cash shop view policy\n";
    return ok ? 0 : 1;
}
