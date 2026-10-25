#pragma once

#include <cstdint>

// Pet capability policy.
//
// Any summoned pet (is_pet_item, item ids 5000000-5000999) intrinsically
// provides: meso magnet, item pouch, wing boots, and auto HP/MP potion
// pouches. Equipped pet-ability items still grant their ability
// independently, and magic scales remains equipment-only (it must actually
// be equipped).

namespace jrc::pet_capability
{
    constexpr int32_t STARTER_PUPPY = 5000001;

    // Returns true when item_id is a summonable pet item (5000000-5000999).
    // Any purchased pet summoned from the cash shop counts: the client treats
    // every pet id as carrying the intrinsic conveniences below.
    constexpr bool is_pet_item(int32_t item_id)
    {
        return item_id / 1000 == 5000;
    }

    enum Mask : uint8_t
    {
        MESO_MAGNET = 1 << 0,
        ITEM_POUCH = 1 << 1,
        AUTO_HP_POUCH = 1 << 2,
        AUTO_MP_POUCH = 1 << 3,
        WING_BOOTS = 1 << 4,
        MAGIC_SCALES = 1 << 5
    };

    struct EquippedAbilities
    {
        bool meso_magnet = false;
        bool item_pouch = false;
        bool auto_hp_pouch = false;
        bool auto_mp_pouch = false;
        bool wing_boots = false;
        bool magic_scales = false;
    };

    struct Capabilities
    {
        bool meso_magnet = false;
        bool item_pouch = false;
        bool auto_hp_pouch = false;
        bool auto_mp_pouch = false;
        bool wing_boots = false;
        bool magic_scales = false;

        uint8_t mask() const;
    };

    Capabilities resolve(int32_t active_pet_item_id,
        const EquippedAbilities& equipped);
}
