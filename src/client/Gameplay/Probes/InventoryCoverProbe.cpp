// Read-only probe for the cash equip cover mechanism. Worn cash equips are
// parked in hidden EQUIP slots at 100 + eqslot, where they mask the normal
// equip in the same slot (see Player::change_equip). This exposes the raw
// covering item id so e2e checks can compare the live look against the
// server database without touching game logic.
#include "../Stage.h"

#include "../../Character/Inventory/InventoryType.h"

#include <cstdint>

extern "C"
{
    // Item id of the cash equip covering the normal equip slot (1..n),
    // 0 when the slot has no cover.
    int msinventory_cover_equipped(int slot)
    {
        if (slot < 1)
        {
            return 0;
        }

        return jrc::Stage::get().get_player()
            .get_inventory()
            .get_item_id(jrc::InventoryType::EQUIP,
                static_cast<int16_t>(100 + slot));
    }
}
