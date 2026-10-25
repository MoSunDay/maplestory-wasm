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
        // Any summoned pet intrinsically grants magnet/pouch/boots/auto-potion
        // conveniences; equipped items only add on top. Magic scales stays
        // equipment-only. STARTER_PUPPY is retained for compatibility.
        const bool pet = is_pet_item(active_pet_item_id);
        return {
            equipped.meso_magnet || pet,
            equipped.item_pouch || pet,
            equipped.auto_hp_pouch || pet,
            equipped.auto_mp_pouch || pet,
            equipped.wing_boots || pet,
            equipped.magic_scales
        };
    }
}
