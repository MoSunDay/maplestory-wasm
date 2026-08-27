#include "PetCapabilityPolicy.h"

namespace jrc::pet_capability
{
    uint8_t Capabilities::mask() const
    {
        return static_cast<uint8_t>(
            (meso_magnet ? MESO_MAGNET : 0) |
            (item_pouch ? ITEM_POUCH : 0) |
            (auto_hp_pouch ? AUTO_HP_POUCH : 0) |
            (auto_mp_pouch ? AUTO_MP_POUCH : 0) |
            (wing_boots ? WING_BOOTS : 0) |
            (magic_scales ? MAGIC_SCALES : 0));
    }

    Capabilities resolve(int32_t active_pet_item_id,
        const EquippedAbilities& equipped)
    {
        const bool starter_puppy = active_pet_item_id == STARTER_PUPPY;
        return {
            equipped.meso_magnet || starter_puppy,
            equipped.item_pouch || starter_puppy,
            equipped.auto_hp_pouch,
            equipped.auto_mp_pouch,
            equipped.wing_boots || starter_puppy,
            equipped.magic_scales
        };
    }
}
