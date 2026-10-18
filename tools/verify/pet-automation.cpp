#include "../../src/client/Gameplay/Pet/PetAutomation.h"
#include "../../src/client/Gameplay/Pet/PetCapabilityPolicy.h"
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

    using jrc::pet_capability::Capabilities;
    using jrc::pet_capability::EquippedAbilities;
    Capabilities starter = jrc::pet_capability::resolve(
        jrc::pet_capability::STARTER_PUPPY, EquippedAbilities{});
    ok &= expect(starter.meso_magnet && starter.item_pouch && starter.wing_boots,
        "starter puppy has intrinsic pickup and movement abilities");
    ok &= expect(!starter.auto_hp_pouch && !starter.auto_mp_pouch &&
        !starter.magic_scales, "starter puppy does not gain unrelated abilities");
    ok &= expect(starter.mask() == (jrc::pet_capability::MESO_MAGNET |
        jrc::pet_capability::ITEM_POUCH | jrc::pet_capability::WING_BOOTS),
        "starter capability mask is deterministic");
    Capabilities ordinary = jrc::pet_capability::resolve(5000002,
        EquippedAbilities{false, true, true, false, false, true});
    ok &= expect(!ordinary.meso_magnet && ordinary.item_pouch &&
        ordinary.auto_hp_pouch && ordinary.magic_scales && !ordinary.wing_boots,
        "ordinary pets retain equipment-only abilities");

    using jrc::pet_follow::Action;
    using jrc::pet_follow::Inputs;
    using jrc::pet_follow::JumpInputs;
    ok &= expect(jrc::pet_follow::GROUND_WALK_FORCE == 0.16,
        "ground pet matches normal walk force");
    ok &= expect(jrc::pet_follow::JUMP_HORIZONTAL_SPEED == 1.25,
        "stalled jump retains normal horizontal travel speed");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 50, 0 }, { 50, 0 }, false, false, 0}) ==
        Action::IDLE, "follow start deadzone");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 50, 0 }, { 50, 0 }, false, true, 0}) ==
        Action::MOVE_RIGHT, "follow movement hysteresis");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { -80, 0 }, { -80, 0 }, false, false, 0}) ==
        Action::MOVE_LEFT, "follow direction");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, 151 }, true, true, 0}) ==
        Action::ABANDON_LOOT, "unreachable loot is abandoned without teleporting");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 40, 0 }, true, true, 3}) ==
        Action::ABANDON_LOOT, "three failed loot jumps abandon the target");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 300, 0 }, { 300, 0 }, false, false, 0}) !=
        Action::WARP_TO_OWNER,
        "owner leash includes exactly 300 pixels");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 301, 0 }, { 301, 0 }, false, false, 0}) ==
        Action::WARP_TO_OWNER,
        "owner leash warps beyond 300 pixels");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 200 }, { 0, 200 }, false, false, 0}) !=
        Action::WARP_TO_OWNER,
        "vertical separation alone does not bypass the 300 pixel leash");
    ok &= expect(jrc::pet_follow::decide(Inputs{{ -32768, -32768 },
        { 32767, 32767 }, { 32767, 32767 }, false, false, 0}) ==
        Action::WARP_TO_OWNER,
        "follow distance remains safe at coordinate extremes");
    ok &= expect(jrc::pet_follow::distance_between(
        { -32768, -32768 }, { 32767, 32767 }) == 92680,
        "reported follow distance remains safe at coordinate extremes");
    ok &= expect(jrc::pet_follow::player_side_position({ -32768, 12 }) ==
        jrc::Point<int16_t>(-32768, 12), "player-side warp destination is clamped");
    ok &= expect(jrc::pet_follow::made_progress(120, 116),
        "moving at least four pixels closer counts as progress");
    ok &= expect(!jrc::pet_follow::made_progress(120, 121),
        "moving without getting closer does not mask a stall");
    ok &= expect(jrc::pet_follow::should_jump(JumpInputs{
        Action::MOVE_RIGHT, true, true, 100.0, 100.0}),
        "grounded right movement blocked without progress triggers jump");
    ok &= expect(jrc::pet_follow::should_jump(JumpInputs{
        Action::MOVE_LEFT, true, true, 100.0, 100.0}),
        "grounded left movement blocked without progress triggers jump");
    ok &= expect(!jrc::pet_follow::should_jump(JumpInputs{
        Action::MOVE_RIGHT, true, true, 99.5, 100.0}),
        "partial movement to a wall waits for a fully blocked step");
    ok &= expect(!jrc::pet_follow::should_jump(JumpInputs{
        Action::MOVE_RIGHT, true, false, 100.0, 100.0}),
        "distance stagnation without collision never triggers jump");
    ok &= expect(!jrc::pet_follow::should_jump(JumpInputs{
        Action::MOVE_RIGHT, false, true, 100.0, 100.0}),
        "airborne collision never triggers a new jump");
    ok &= expect(!jrc::pet_follow::should_jump(JumpInputs{
        Action::IDLE, true, true, 100.0, 100.0}),
        "idle collision never triggers jump");
    const jrc::pet_follow::JumpProgress launch =
        jrc::pet_follow::advance_jump(false, true);
    const jrc::pet_follow::JumpProgress airborne =
        jrc::pet_follow::advance_jump(launch.airborne, false);
    const jrc::pet_follow::JumpProgress landing =
        jrc::pet_follow::advance_jump(airborne.airborne, true);
    ok &= expect(!launch.airborne && !launch.landed,
        "launch tick does not finish against stale ground state");
    ok &= expect(airborne.airborne && !airborne.landed,
        "airborne tick arms landing detection");
    ok &= expect(landing.airborne && landing.landed,
        "grounded tick after flight completes the jump");
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
        std::cout << "PASS pet capabilities, automation, pickup, and leash rules\n";
    return ok ? 0 : 1;
}
