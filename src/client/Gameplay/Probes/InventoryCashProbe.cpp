#include "../Stage.h"

#include "../../Character/Inventory/InventoryType.h"

#include <cstdint>

extern "C"
{
    // Cash items share item ids across slots, so the caller supplies the
    // unique cash id (pet id included) instead of the item id.
    int msinventory_cash_slot(int32_t cash_id)
    {
        if (cash_id == 0)
        {
            return 0;
        }

        const auto& inventory = jrc::Stage::get().get_player().get_inventory();
        for (const auto& item : inventory.get_cash_items())
        {
            if (item.type == jrc::InventoryType::CASH &&
                item.cash_id == static_cast<int64_t>(cash_id))
            {
                return item.slot;
            }
        }
        return 0;
    }
}
