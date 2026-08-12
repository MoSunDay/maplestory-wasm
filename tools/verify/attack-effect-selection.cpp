#include <cassert>

#include "Gameplay/Combat/Effects/AttackEffectSelection.h"
#include "Gameplay/Input/ActionTrigger.h"

int main()
{
    using jrc::attack_effect::HitIndex;

    assert(jrc::action_trigger::pressed_once(true, false));
    assert(!jrc::action_trigger::pressed_once(true, true));
    assert(!jrc::action_trigger::pressed_once(false, true));
    assert(!jrc::action_trigger::pressed_once(false, false));

    assert(!jrc::attack_effect::indexed_hit(1, 1, 1));
    assert(jrc::attack_effect::indexed_hit(2, 2, 6) == HitIndex::HIT);
    assert(jrc::attack_effect::indexed_hit(6, 1, 6) == HitIndex::TARGET);
    assert(jrc::attack_effect::indexed_hit(2, 2, 2) == HitIndex::HIT);
    assert(!jrc::attack_effect::indexed_hit(2, 1, 6));

    assert(jrc::attack_effect::bounded_variant(0, 0) == 0);
    assert(jrc::attack_effect::bounded_variant(1, 3) == 1);
    assert(jrc::attack_effect::bounded_variant(9, 3) == 2);

    assert(jrc::attack_effect::consumable_combo_orbs(0) == 0);
    assert(jrc::attack_effect::consumable_combo_orbs(1) == 0);
    assert(jrc::attack_effect::consumable_combo_orbs(10) == 9);

    using namespace jrc::SkillId;
    assert(jrc::attack_effect::charged_blow_element(SWORD_FIRE_CHARGE) == 1);
    assert(jrc::attack_effect::charged_blow_element(BW_ICE_CHARGE) == 2);
    assert(jrc::attack_effect::charged_blow_element(SWORD_LIT_CHARGE) == 3);
    assert(jrc::attack_effect::charged_blow_element(BW_HOLY_CHARGE) == 5);
    assert(jrc::attack_effect::charged_blow_element(0) == 0);
}
