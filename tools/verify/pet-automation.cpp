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
    ok &= expect(jrc::pet_capability::is_pet_item(
        jrc::pet_capability::STARTER_PUPPY), "starter puppy is a pet item");
    ok &= expect(jrc::pet_capability::is_pet_item(5000000) &&
        jrc::pet_capability::is_pet_item(5000999),
        "pet item id range covers 5000000-5000999");
    ok &= expect(!jrc::pet_capability::is_pet_item(4999999) &&
        !jrc::pet_capability::is_pet_item(5001000),
        "ids outside the pet range are rejected");
    Capabilities starter = jrc::pet_capability::resolve(
        jrc::pet_capability::STARTER_PUPPY, EquippedAbilities{});
    ok &= expect(starter.meso_magnet && starter.item_pouch && starter.wing_boots,
        "starter puppy has intrinsic pickup and movement abilities");
    ok &= expect(starter.auto_hp_pouch && starter.auto_mp_pouch,
        "starter puppy has intrinsic auto potion pouches");
    ok &= expect(!starter.magic_scales,
        "starter puppy does not gain magic scales");
    ok &= expect(starter.mask() == (jrc::pet_capability::MESO_MAGNET |
        jrc::pet_capability::ITEM_POUCH | jrc::pet_capability::AUTO_HP_POUCH |
        jrc::pet_capability::AUTO_MP_POUCH | jrc::pet_capability::WING_BOOTS),
        "starter capability mask is deterministic");
    Capabilities ordinary = jrc::pet_capability::resolve(5000002,
        EquippedAbilities{});
    ok &= expect(ordinary.meso_magnet && ordinary.item_pouch &&
        ordinary.auto_hp_pouch && ordinary.auto_mp_pouch &&
        ordinary.wing_boots && !ordinary.magic_scales,
        "ordinary pets have every intrinsic automation ability");
    Capabilities equipped = jrc::pet_capability::resolve(5000002,
        EquippedAbilities{true, true, true, true, true, true});
    ok &= expect(equipped.meso_magnet && equipped.item_pouch &&
        equipped.auto_hp_pouch && equipped.auto_mp_pouch &&
        equipped.wing_boots && equipped.magic_scales,
        "equipped abilities still stack on an ordinary pet");
    Capabilities nonpet_none = jrc::pet_capability::resolve(0,
        EquippedAbilities{});
    Capabilities nonpet_item = jrc::pet_capability::resolve(1812000,
        EquippedAbilities{});
    ok &= expect(!nonpet_none.meso_magnet && !nonpet_none.item_pouch &&
        !nonpet_none.auto_hp_pouch && !nonpet_none.auto_mp_pouch &&
        !nonpet_none.wing_boots && !nonpet_none.magic_scales &&
        !nonpet_item.meso_magnet && !nonpet_item.item_pouch &&
        !nonpet_item.auto_hp_pouch && !nonpet_item.auto_mp_pouch &&
        !nonpet_item.wing_boots && !nonpet_item.magic_scales,
        "non-pet ids without equipment gain no abilities");

    using jrc::pet_follow::Action;
    using jrc::pet_follow::Inputs;
    using jrc::pet_follow::JumpInputs;
    ok &= expect(jrc::pet_follow::BASE_MOVEMENT.walk_force == 0.16,
        "base pet profile matches normal walk force");
    ok &= expect(jrc::pet_follow::BASE_MOVEMENT.jump_force == 4.5,
        "base pet profile matches normal jump force");
    ok &= expect(jrc::pet_follow::jump_reach_height(4.5, 0.14) == 72,
        "jump reach height follows the shared gravity");
    ok &= expect(jrc::pet_follow::pursuit_height_limit(
        jrc::pet_follow::BASE_MOVEMENT) == 88,
        "pursuit height limit is the owner jump reach plus tolerance");
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
        { 0, 0 }, { 0, 0 }, { 10, -88 }, true, true, 0}) ==
        Action::IDLE,
        "parked loot within jump reach stays pursued");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, -89 }, true, true, 0}) ==
        Action::ABANDON_LOOT,
        "loot above the owner jump reach is abandoned");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, 240 }, true, true, 0}) ==
        Action::IDLE,
        "parked loot within the fall guard stays pursued");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, 16 }, true, true, 0}) == Action::IDLE,
        "loot inside pickup range is still pursued while parked");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 5, 100 }, true, false, 0}) ==
        Action::IDLE,
        "standing pet waits beside loot below");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, 240 }, true, true, 0, 88 }) ==
        Action::IDLE,
        "deep loot pursuit continues while parked");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, -60 }, true, true, 0, 88 }) ==
        Action::IDLE,
        "high loot pursuit continues while parked");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 60, 16 }, true, true, 0}) ==
        Action::MOVE_RIGHT,
        "walking pursuit of distant loot is not interrupted");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, 0 }, { 10, 241 }, true, true, 0}) ==
        Action::ABANDON_LOOT,
        "loot below beyond the fall guard is abandoned");
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
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, -200 }, { 0, -200 }, false, false, 0}) ==
        Action::WARP_TO_OWNER,
        "owner 200px overhead warps back instead of crawling below");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, -88 }, { 0, -88 }, false, false, 0}) !=
        Action::WARP_TO_OWNER,
        "owner within the pursuit height overhead keeps walking");
    ok &= expect(jrc::pet_follow::decide(Inputs{
        { 0, 0 }, { 0, -200 }, { 60, 0 }, true, false, 0}) ==
        Action::MOVE_RIGHT,
        "loot pursuit suppresses the overhead warp");
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
    ok &= expect(jrc::pet_loot::BATCH_RANGE == 50,
        "pet pickup sweep stays inside the server range check");
    ok &= expect(jrc::pet_loot::batch_count(0) == 0 &&
        jrc::pet_loot::batch_count(2) == 2 &&
        jrc::pet_loot::batch_count(9) ==
            jrc::pet_loot::BATCH_LIMIT,
        "pickup sweeps cap the batch size but keep small clusters whole");
    ok &= expect(jrc::pet_loot::in_reach({ 0, 0 }, { 50, 16 }, 88),
        "adjacent drop below the foothold is in reach");
    ok &= expect(jrc::pet_loot::in_reach({ 0, 0 }, { 10, 96 }, 88),
        "drop one ledge below the pet is in reach");
    ok &= expect(!jrc::pet_loot::in_reach({ 0, 0 }, { 10, 97 }, 88),
        "drop deeper than the reach window is out of reach");
    ok &= expect(!jrc::pet_loot::in_reach({ 0, 0 }, { 51, 16 }, 88),
        "horizontal reach ends at the sweep window");
    ok &= expect(jrc::pet_loot::in_reach({ 0, 0 }, { 10, -88 }, 88),
        "drop at the pursuit height limit is in reach");
    ok &= expect(!jrc::pet_loot::in_reach({ 0, 0 }, { 10, -89 }, 88),
        "drop above the pursuit height limit is out of reach");
    ok &= expect(!jrc::pet_loot::should_abandon(239, false),
        "pursuit keeps trying before the abandon window");
    ok &= expect(jrc::pet_loot::should_abandon(240, false),
        "pursuit gives up once the abandon window passes");
    ok &= expect(!jrc::pet_loot::should_abandon(1000, true),
        "reached loot is never abandoned");

    if (ok)
        std::cout << "PASS pet capabilities, automation, pickup, and leash rules\n";
    return ok ? 0 : 1;
}
