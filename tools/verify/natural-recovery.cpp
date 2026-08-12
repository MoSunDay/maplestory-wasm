#include "Character/Recovery/NaturalRecovery.h"

#include <cassert>

using jrc::natural_recovery::Context;
using jrc::natural_recovery::Posture;
using jrc::natural_recovery::State;
using jrc::natural_recovery::advance;
using jrc::natural_recovery::hp_amount;
using jrc::natural_recovery::mp_amount;

int main()
{
    Context standing;
    standing.posture = Posture::STANDING;
    standing.hp = 10;
    standing.max_hp = 100;
    standing.mp = 10;
    standing.max_mp = 100;

    auto before_interval = advance({}, standing, 9999);
    assert(before_interval.hp_gain == 0);
    assert(before_interval.mp_gain == 0);

    auto normal_tick = advance(before_interval.state, standing, 1);
    assert(normal_tick.hp_gain == 10);
    assert(normal_tick.mp_gain == 3);
    assert(normal_tick.state.hp_elapsed_ms == 0);
    assert(normal_tick.state.mp_elapsed_ms == 0);

    Context skilled = standing;
    skilled.improved_hp = 20;
    skilled.improved_mp = 17;
    assert(hp_amount(skilled) == 30);
    assert(mp_amount(skilled) == 20);

    skilled.level = 30;
    skilled.magician_mp_skill_level = 10;
    assert(mp_amount(skilled) == 33);

    Context chair = skilled;
    chair.posture = Posture::SITTING;
    chair.chair_hp = 50;
    chair.chair_mp = 25;
    assert(hp_amount(chair) == 50);
    assert(mp_amount(chair) == 33);

    chair.chair_hp = 1000;
    chair.chair_mp = 1000;
    chair.map_recovery = 2.0f;
    assert(hp_amount(chair) == 120);
    assert(mp_amount(chair) == 999);

    Context disabled_map = chair;
    disabled_map.map_recovery = 0.0f;
    assert(hp_amount(disabled_map) == 0);
    assert(mp_amount(disabled_map) == 0);

    Context climbing = standing;
    climbing.posture = Posture::CLIMBING;
    climbing.endure_interval_seconds = 3;
    auto climbing_tick = advance({}, climbing, 3000);
    assert(climbing_tick.hp_gain == 10);
    assert(climbing_tick.mp_gain == 0);

    Context moving = standing;
    moving.posture = Posture::OTHER;
    auto moving_tick = advance({}, moving, 10000);
    assert(moving_tick.hp_gain == 0);
    assert(moving_tick.mp_gain == 3);
    assert(moving_tick.state.hp_elapsed_ms == 0);

    Context full = standing;
    full.hp = full.max_hp;
    full.mp = full.max_mp;
    auto full_tick = advance({}, full, 10000);
    assert(full_tick.hp_gain == 0);
    assert(full_tick.mp_gain == 0);

    Context dead = standing;
    dead.posture = Posture::DEAD;
    auto dead_tick = advance({ 5000, 5000 }, dead, 1000);
    assert(dead_tick.state.hp_elapsed_ms == 0);
    assert(dead_tick.state.mp_elapsed_ms == 0);
}
