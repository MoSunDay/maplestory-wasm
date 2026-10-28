// Read-only runtime probes for the equipment settlement chain. They expose
// raw numeric state so e2e verification can compare the live client against
// the server database without touching game logic.
#include "../Stage.h"

#include "../../Character/EquipStat.h"
#include "../../Character/Inventory/InventoryType.h"

#include <cstdint>

extern "C"
{
    // Item id at an inventory slot, 0 when empty (defined below).
    int msinv_slot_item(int invtype, int slot);

    // Number of occupied slots in an inventory type (-1 EQUIPPED .. 5 CASH).
    // Equipped slots are positive EquipSlot ids; pack slots are 1..N, so a
    // sweep over the union range counts both layouts.
    int msinv_count(int invtype)
    {
        int count = 0;
        for (int slot = -1; slot <= 120; ++slot)
        {
            if (msinv_slot_item(invtype, slot) != 0)
            {
                ++count;
            }
        }
        return count;
    }

    // Item id at an inventory slot, 0 when empty.
    int msinv_slot_item(int invtype, int slot)
    {
        return jrc::Stage::get().get_player()
            .get_inventory()
            .get_item_id(static_cast<jrc::InventoryType::Id>(invtype),
                static_cast<int16_t>(slot));
    }

    // CharStats total for an Equipstat id (STR..JUMP), including equips and
    // buffs. Returns -1 for an unknown stat id.
    int msstat_total(int stat)
    {
        if (stat < 0 || stat >= static_cast<int>(jrc::Equipstat::LENGTH))
        {
            return -1;
        }
        const jrc::Equipstat::Id id =
            jrc::Equipstat::by_id(static_cast<size_t>(stat));
        return jrc::Stage::get().get_player().get_stats().get_total(id);
    }
    // Damage the live client would take from a mob attack (magic selects
    // MDEF, physical WDEF). Exposes calculate_damage for wire-level
    // verification that defense actually participates in the settlement.
    int msstat_damage(int mobatk, int magic)
    {
        return jrc::Stage::get().get_player().get_stats()
            .calculate_damage(mobatk, magic != 0);
    }
}
