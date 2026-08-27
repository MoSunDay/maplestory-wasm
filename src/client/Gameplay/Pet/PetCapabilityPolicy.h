#pragma once

#include <cstdint>

namespace jrc::pet_capability
{
    constexpr int32_t STARTER_PUPPY = 5000001;

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
