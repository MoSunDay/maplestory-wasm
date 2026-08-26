#include "../../src/client/Gameplay/Pet/PetAutomation.h"
#include "../../src/client/Gameplay/Pet/PetFollowPolicy.h"
#include "../../src/client/Gameplay/Pet/PetLootPolicy.h"

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
    using namespace jrc::pet_automation;
    bool ok = true;
    ok &= expect(clamp_threshold(-10) == 1, "threshold lower bound");
    ok &= expect(clamp_threshold(120) == 99, "threshold upper bound");
    ok &= expect(at_or_below_threshold(50, 100, 50), "inclusive threshold");
    ok &= expect(!at_or_below_threshold(51, 100, 50), "above threshold");
    ok &= expect(!at_or_below_threshold(1, 0, 50), "invalid maximum");

    std::vector<int32_t> potions = { 0, 2000000, 2000001 };
    ok &= expect(select_next_item(potions, 0, 1) == 2000000,
        "select next potion");
    ok &= expect(select_next_item(potions, 0, -1) == 2000001,
        "potion selection wraps backwards");
    ok &= expect(select_next_item(potions, 9999999, 1) == 0,
        "missing potion resets to disabled");

    Controller controller(50, 40);
    controller.set_hp_item(2000000);
    controller.set_mp_item(2000001);
    AutoPotInputs inputs = {
        50, 100, 40, 100, true, true, true, true, true, true
    };
    AutoPotDecisions first = controller.decide(inputs);
    ok &= expect(first.count == 1, "one potion per decision");
    ok &= expect(first.requests[0].resource == Resource::HP, "HP priority");
    ok &= expect(first.requests[0].item_id == 2000000, "HP item selected");

    AutoPotDecisions second = controller.decide(inputs);
    ok &= expect(second.count == 1, "MP follows while HP cools down");
    ok &= expect(second.requests[0].resource == Resource::MP, "MP request");

    Controller combined(50, 50);
    combined.set_hp_item(2020015);
    combined.set_mp_item(2020015);
    AutoPotDecisions both = combined.decide(inputs);
    ok &= expect(both.count == 1, "combined potion is not duplicated");
    ok &= expect(both.requests[0].resource == Resource::BOTH, "combined resource");

    inputs.pet_active = false;
    Controller inactive;
    inactive.set_hp_item(2000000);
    ok &= expect(inactive.decide(inputs).count == 0, "inactive pet blocks auto potion");

    using jrc::pet_follow::Action;
    using jrc::pet_follow::Inputs;
    ok &= expect(jrc::pet_follow::decide(Inputs{{ 0, 0 }, { 50, 0 }, false, false, 0}) ==
        Action::IDLE, "follow start deadzone");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ 0, 0 }, { 50, 0 }, false, true, 0}) ==
        Action::MOVE_RIGHT, "follow movement hysteresis");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ 0, 0 }, { -80, 0 }, false, false, 0}) ==
        Action::MOVE_LEFT, "follow direction");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ 0, 0 }, { 10, 80 }, true, true, 0}) ==
        Action::WARP, "unreachable loot warps");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ 0, 0 }, { 40, 0 }, true, true, 90}) ==
        Action::WARP, "stalled pet recovers");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ -32768, -32768 },
        { 32767, 32767 }, false, false, 0}) == Action::WARP,
        "follow distance remains safe at coordinate extremes");
    ok &= expect(!jrc::pet_loot::should_attempt(50, 0, true),
        "server drop lock is respected");
    ok &= expect(jrc::pet_loot::should_attempt(55, 0, true),
        "first pickup starts after server lock");
    ok &= expect(!jrc::pet_loot::should_attempt(79, 1, true) &&
        jrc::pet_loot::should_attempt(80, 1, true), "pickup retries are spaced");
    ok &= expect(!jrc::pet_loot::should_attempt(200, 0, false),
        "out-of-range pet cannot request pickup");
    ok &= expect(jrc::pet_loot::attempts_exhausted(3),
        "failed target is eventually abandoned");

    if (ok)
        std::cout << "PASS pet potion selection, automation, and stable follow rules\n";
    return ok ? 0 : 1;
}
