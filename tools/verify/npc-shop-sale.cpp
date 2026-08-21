#include <cassert>
#include <string>

#include "IO/UITypes/Notice/NumberInputPolicy.h"
#include "IO/UITypes/Shop/SalePolicy.h"

int main()
{
    using jrc::InventoryType::CASH;
    using jrc::InventoryType::EQUIP;
    using jrc::InventoryType::ETC;
    using jrc::InventoryType::SETUP;
    using jrc::InventoryType::USE;

    assert(jrc::number_input_policy::clamp_maximum("", 100).empty());
    assert(jrc::number_input_policy::clamp_maximum("99", 100) == "99");
    assert(jrc::number_input_policy::clamp_maximum("100", 100) == "100");
    assert(jrc::number_input_policy::clamp_maximum("101", 100) == "100");
    assert(jrc::number_input_policy::clamp_maximum("999999999999999999", 100) == "100");
    assert(jrc::number_input_policy::clamp_maximum("12x", 100) == "12x");

    assert(jrc::shop_sale_policy::supports_bulk_sale(EQUIP));
    assert(jrc::shop_sale_policy::supports_bulk_sale(USE));
    assert(jrc::shop_sale_policy::supports_bulk_sale(SETUP));
    assert(jrc::shop_sale_policy::supports_bulk_sale(ETC));
    assert(!jrc::shop_sale_policy::supports_bulk_sale(CASH));
    assert(jrc::shop_sale_policy::is_visible_sale_item(USE, false));
    assert(!jrc::shop_sale_policy::is_visible_sale_item(USE, true));
    assert(!jrc::shop_sale_policy::is_visible_sale_item(CASH, false));
}
