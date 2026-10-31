#include "../../src/client/Gameplay/Pet/PetVitalityPolicy.h"

#include <iostream>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }
}

int main()
{
    using namespace jrc::pet_vitality;
    bool ok = true;

    ok &= expect(HUNGRY_THRESHOLD == 40, "threshold is 40");
    ok &= expect(is_hungry(0), "zero fullness is hungry");
    ok &= expect(is_hungry(39), "fullness 39 is hungry");
    ok &= expect(!is_hungry(40), "fullness 40 is not hungry");
    ok &= expect(!is_hungry(100), "fullness 100 is not hungry");

    ok &= expect(classify(false, 0) == State::NO_PET, "no summoned pet");
    ok &= expect(classify(true, 0) == State::HUNGRY, "summoned starving pet");
    ok &= expect(classify(true, 39) == State::HUNGRY, "summoned hungry pet");
    ok &= expect(classify(true, 40) == State::SATISFIED, "summoned fed pet");
    ok &= expect(classify(true, 100) == State::SATISFIED, "summoned full pet");

    // The linked server despawns a starving pet and resets fullness to 15:
    // still hungry, which guides the player towards feeding it.
    ok &= expect(is_hungry(15), "post-despawn fullness is still hungry");

    ok &= expect(is_pet_food(2120000), "pet food group low id");
    ok &= expect(is_pet_food(2129999), "pet food group high id");
    ok &= expect(!is_pet_food(2119999), "below pet food group");
    ok &= expect(!is_pet_food(2130000), "above pet food group");
    ok &= expect(!is_pet_food(2000000), "potion is not pet food");
    ok &= expect(!is_pet_food(0), "zero id is not pet food");

    ok &= expect(describe(State::NO_PET) == "No pet summoned", "no pet text");
    ok &= expect(describe(State::HUNGRY) == "HUNGRY!", "hungry text");
    ok &= expect(describe(State::SATISFIED) == "Satisfied", "satisfied text");

    return ok ? 0 : 1;
}
