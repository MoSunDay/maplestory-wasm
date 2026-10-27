// Item inventory icon probe, split from GameplayProbes.cpp to respect the
// per-file line budget.
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIItemInventory.h"

#include <cstdint>

extern "C"
{
    // Item id of the icon shown at a visible inventory slot. Returns 0 when
    // the slot is empty or the inventory is closed.
    int msiteminventory_icon_item_id(int16_t slot)
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->item_id_at(slot) : 0;
    }
}
