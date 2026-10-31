#pragma once

#include <cstdint>
#include <string>

// Pet vitality policy.
//
// A summoned pet's fullness drains over time on the server. Below the
// hungry threshold a pet keeps following its owner but refuses to work:
// no automatic looting and no automatic potion use. Feeding pet food
// (item ids 2120000-2129999) restores fullness and puts it back to work.

namespace jrc::pet_vitality
{
    // Fullness below this value counts as hungry. Client-side policy; the
    // linked server force-despawns pets once fullness drops to <= 5.
    constexpr uint8_t HUNGRY_THRESHOLD = 40;

    enum class State : uint8_t
    {
        NO_PET,
        SATISFIED,
        HUNGRY
    };

    // Returns true when the fullness value is in the hungry range.
    bool is_hungry(uint8_t fullness);

    // Classify the vitality of a (possibly missing) summoned pet.
    State classify(bool summoned, uint8_t fullness);

    // Returns true when item_id is pet food (2120000-2129999).
    bool is_pet_food(int32_t item_id);

    // Human-readable short status text for UI display.
    std::string describe(State state);
}
