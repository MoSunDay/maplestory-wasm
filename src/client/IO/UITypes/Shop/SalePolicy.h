#pragma once

#include "../../../Character/Inventory/InventoryType.h"

namespace jrc::shop_sale_policy
{
    constexpr bool supports_bulk_sale(InventoryType::Id type)
    {
        return type == InventoryType::EQUIP || type == InventoryType::USE ||
            type == InventoryType::SETUP || type == InventoryType::ETC;
    }

    constexpr bool is_visible_sale_item(InventoryType::Id type, bool cash)
    {
        return supports_bulk_sale(type) && !cash;
    }
}
