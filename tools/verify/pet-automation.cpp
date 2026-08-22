#include "../../src/client/Gameplay/Pet/PetAutomation.h"

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

    if (ok)
        std::cout << "PASS pet automation threshold, ability, cooldown, and coalescing rules\n";
    return ok ? 0 : 1;
}
