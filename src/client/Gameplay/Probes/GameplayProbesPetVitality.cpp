// Pet vitality probes, split from GameplayProbes.cpp to respect the
// per-file line budget.
#include "../Stage.h"

#include <cstdint>

extern "C"
{
    // Returns 1 when the lead summoned pet is hungry (fullness below the
    // pet vitality threshold), 0 otherwise (including no summoned pet).
    int mspet_hungry()
    {
        return jrc::Stage::get().is_pet_hungry() ? 1 : 0;
    }

    // Fullness of the lead summoned pet, or -1 when it is unknown (no pet
    // summoned or no stored stats for its cash id).
    int mspet_fullness()
    {
        const jrc::Player& player = jrc::Stage::get().get_player();
        const jrc::PetLook* pet = player.get_pet(0);
        if (!pet)
            return -1;

        const jrc::Pet* petinfo =
            player.get_inventory().get_pet_by_cash_id(pet->get_uniqueid());
        return petinfo ? petinfo->get_fullness() : -1;
    }

    // Level of the lead summoned pet, or -1 when it is unknown.
    int mspet_level()
    {
        const jrc::Player& player = jrc::Stage::get().get_player();
        const jrc::PetLook* pet = player.get_pet(0);
        if (!pet)
            return -1;

        const jrc::Pet* petinfo =
            player.get_inventory().get_pet_by_cash_id(pet->get_uniqueid());
        return petinfo ? petinfo->get_level() : -1;
    }
}
