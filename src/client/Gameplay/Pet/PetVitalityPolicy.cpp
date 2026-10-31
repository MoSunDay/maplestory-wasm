#include "PetVitalityPolicy.h"

namespace jrc::pet_vitality
{
    bool is_hungry(uint8_t fullness)
    {
        return fullness < HUNGRY_THRESHOLD;
    }

    State classify(bool summoned, uint8_t fullness)
    {
        if (!summoned)
            return State::NO_PET;

        return is_hungry(fullness) ? State::HUNGRY : State::SATISFIED;
    }

    bool is_pet_food(int32_t item_id)
    {
        return item_id / 10000 == 212;
    }

    std::string describe(State state)
    {
        switch (state)
        {
        case State::NO_PET:
            return "No pet summoned";
        case State::HUNGRY:
            return "HUNGRY!";
        default:
            return "Satisfied";
        }
    }
}
