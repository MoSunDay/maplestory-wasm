#include "../../src/client/Character/Buff.h"
#include "../../src/client/Gameplay/Pet/PetAutomation.h"
#include "../../src/client/Gameplay/Pet/PetBuffPolicy.h"

#include <iostream>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }

    // Learned, non-passive/attack/summon skill with level data available.
    // The remaining fields describe the skill's NX shape.
    jrc::pet_buff::PolicyInput shape(int32_t skill_id, int32_t skill_type,
        bool effect, bool hit, bool ball, bool alert2, bool mount = false)
    {
        return jrc::pet_buff::PolicyInput{ 1, false, false, false, true,
            skill_id, skill_type, effect, hit, ball, alert2, mount };
    }

    // Minimal SkillData-like stand-in exercising the admission template.
    struct MockSkill
    {
        int32_t skill_type = -1;
        bool effect = false;
        bool hit = false;
        bool ball = false;
        bool alert2 = false;
        int32_t masterlevel = 30;
        bool passive = false;
        bool attack = false;
        bool summon = false;

        bool is_passive() const { return passive; }
        bool is_attack() const { return attack; }
        bool is_summon() const { return summon; }
        int32_t get_masterlevel() const { return masterlevel; }
        int32_t get_skill_type() const { return skill_type; }
        bool has_effect() const { return effect; }
        bool has_hit() const { return hit; }
        bool has_ball() const { return ball; }
        bool has_alert2_action() const { return alert2; }
    };
}

int main()
{
    using jrc::pet_buff::PolicyInput;
    using jrc::pet_buff::pet_buff_skill_allowed;
    using namespace jrc::pet_automation;

    bool ok = true;
    ok &= expect(pet_buff_skill_allowed(shape(4101004, -1, true, false,
        false, false)),
        "valid buff skill");
    ok &= expect(!pet_buff_skill_allowed(PolicyInput{ 0, false, false, false,
        true }),
        "zero level rejected");
    ok &= expect(!pet_buff_skill_allowed(PolicyInput{ 1, true, false, false,
        true }),
        "passive rejected");
    ok &= expect(!pet_buff_skill_allowed(PolicyInput{ 1, false, true, false,
        true }),
        "attack rejected");
    ok &= expect(!pet_buff_skill_allowed(PolicyInput{ 1, false, false, true,
        true }),
        "summon rejected");
    ok &= expect(!pet_buff_skill_allowed(PolicyInput{ 1, false, false, false,
        false }),
        "missing level data rejected");

    // skillType gates the heuristic outright when present.
    ok &= expect(pet_buff_skill_allowed(shape(1101004, 2, false, false,
        false, false)),
        "skillType 2 booster allowed");
    ok &= expect(!pet_buff_skill_allowed(shape(0, 1, true, false, false,
        false)),
        "skillType 1 rejected");
    ok &= expect(!pet_buff_skill_allowed(shape(0, 3, false, false, false,
        false)),
        "skillType 3 rejected");

    // skillType absent: effect/hit/ball heuristic plus the alert2 pose.
    ok &= expect(pet_buff_skill_allowed(shape(1121000, -1, true, false,
        false, false)),
        "Maple Warrior effect shape allowed");
    ok &= expect(pet_buff_skill_allowed(shape(4101004, -1, true, false,
        false, true)),
        "Haste effect+alert2 shape allowed");
    ok &= expect(!pet_buff_skill_allowed(shape(0, -1, true, true, false,
        false)),
        "effect with hit rejected");
    ok &= expect(!pet_buff_skill_allowed(shape(0, -1, true, false, true,
        false)),
        "effect with ball rejected");
    ok &= expect(!pet_buff_skill_allowed(shape(2101002, -1, false, false,
        false, false)),
        "Teleport no-effect shape rejected");
    ok &= expect(pet_buff_skill_allowed(shape(0, -1, false, false, false,
        true)),
        "alert2 alone allowed");

    // Monster Rider: server GIVE_BUFF carries the mount item id, so a cast
    // can never be confirmed.
    constexpr int32_t MOUNTS[] = { 1004, 10001004, 20001004, 20011004 };
    for (int32_t mount : MOUNTS)
    {
        ok &= expect(!pet_buff_skill_allowed(shape(mount, -1, true, false,
            false, false, true)),
            "mount skill rejected via mount_skill");
        ok &= expect(jrc::pet_buff::is_mount_skill(mount),
            "mount id recognised by is_mount_skill");
    }
    ok &= expect(!jrc::pet_buff::is_mount_skill(1101004),
        "booster is not a mount id");

    // Server override lists (audit 2026-11-15, SkillFactory.java).
    ok &= expect(!pet_buff_skill_allowed(shape(2301002, -1, true, true,
        false, true)),
        "known non-buff Heal rejected despite buff shape");
    ok &= expect(!pet_buff_skill_allowed(shape(1121001, -1, true, false,
        false, false)),
        "known non-buff Monster Magnet rejected");
    ok &= expect(pet_buff_skill_allowed(shape(1001003, -1, false, false,
        false, false)),
        "known buff Iron Body allowed despite empty shape");
    // Attack/proc skills with empty statups: the server sends GIVE_BUFF only
    // when localstatups is non-empty (MapleStatEffect.java :1104-1108), so a
    // pet cast can never be confirmed and would loop on backoff. They left
    // the allowlist and joined neither override table. Their real NX shapes
    // (skillType 3 for FINAL_ATTACK, absent skillType plus effect/hit for
    // the Thunder Breaker procs) now fall through to denial; a hypothetical
    // skillType 2 is NOT the test vector because unknown skillType-2 ids
    // pass generically by design, like any booster.
    constexpr int32_t FA_PROCS[] = { 11101002, 13101002 };
    for (int32_t proc : FA_PROCS)
        ok &= expect(!pet_buff_skill_allowed(shape(proc, 3, true, true,
            false, true)),
            "Final Attack proc rejected despite alert2 pose");
    constexpr int32_t TB_PROCS[] = { 15100004, 15111001, 15111006 };
    for (int32_t proc : TB_PROCS)
    {
        ok &= expect(!pet_buff_skill_allowed(shape(proc, -1, true, true,
            true, false)),
            "Thunder Breaker proc rejected by attack shape");
        PolicyInput proc_attack = shape(proc, -1, true, true, true, false);
        proc_attack.attack = true;
        ok &= expect(!pet_buff_skill_allowed(proc_attack),
            "proc skill rejected by attack gate");
    }
    ok &= expect(jrc::pet_buff::known_nonbuff_skill(2301002) &&
        jrc::pet_buff::known_nonbuff_skill(9101000) &&
        !jrc::pet_buff::known_nonbuff_skill(1101004),
        "known non-buff table contents");
    ok &= expect(jrc::pet_buff::known_buff_skill(1001003) &&
        jrc::pet_buff::known_buff_skill(1320008) &&
        jrc::pet_buff::known_buff_skill(1320009) &&
        jrc::pet_buff::known_buff_skill(5111005) &&
        jrc::pet_buff::known_buff_skill(5121003) &&
        jrc::pet_buff::known_buff_skill(10000012) &&
        jrc::pet_buff::known_buff_skill(13111005) &&
        jrc::pet_buff::known_buff_skill(15111002) &&
        jrc::pet_buff::known_buff_skill(20000012) &&
        jrc::pet_buff::known_buff_skill(21000000),
        "known buff table keeps confirmable statup skills");
    ok &= expect(!jrc::pet_buff::known_buff_skill(11101002) &&
        !jrc::pet_buff::known_buff_skill(13101002) &&
        !jrc::pet_buff::known_buff_skill(15100004) &&
        !jrc::pet_buff::known_buff_skill(15111001) &&
        !jrc::pet_buff::known_buff_skill(15111006),
        "statup-less proc skills dropped from buff table");
    ok &= expect(!jrc::pet_buff::known_nonbuff_skill(11101002) &&
        !jrc::pet_buff::known_nonbuff_skill(13101002) &&
        !jrc::pet_buff::known_nonbuff_skill(15100004) &&
        !jrc::pet_buff::known_nonbuff_skill(15111001) &&
        !jrc::pet_buff::known_nonbuff_skill(15111006),
        "proc skills in neither table, denied by shape fall-through");

    // Shared-stat satisfaction: boosters and charges are multiplexed onto
    // one Buffstat each, so ANY family member satisfies the slot.
    ok &= expect(jrc::pet_buff::buff_slot_satisfied(1121000, true, false,
        false),
        "plain skill satisfied by own buff");
    ok &= expect(!jrc::pet_buff::buff_slot_satisfied(1121000, false, true,
        true),
        "plain skill untouched by shared stats");
    ok &= expect(jrc::pet_buff::buff_slot_satisfied(1101004, false, true,
        false),
        "booster 1101004 satisfied by BOOSTER stat");
    ok &= expect(jrc::pet_buff::buff_slot_satisfied(1101005, false, true,
        false),
        "booster 1101005 satisfied by BOOSTER stat");
    ok &= expect(!jrc::pet_buff::buff_slot_satisfied(1101004, false, false,
        true),
        "booster untouched by WK_CHARGE stat");
    ok &= expect(jrc::pet_buff::buff_slot_satisfied(1211004, false, false,
        true),
        "charge 1211004 satisfied by WK_CHARGE stat");
    ok &= expect(jrc::pet_buff::buff_slot_satisfied(1221004, false, false,
        true),
        "charge 1221004 satisfied by WK_CHARGE stat");
    ok &= expect(!jrc::pet_buff::buff_slot_satisfied(1211004, false, true,
        false),
        "charge untouched by BOOSTER stat");

    // Admission template wiring.
    MockSkill haste;
    haste.effect = true;
    haste.alert2 = true;
    ok &= expect(jrc::pet_buff::skill_can_auto_buff(haste, 4101004, 10),
        "template admits Haste shape");
    ok &= expect(!jrc::pet_buff::skill_can_auto_buff(haste, 4101004, 0),
        "template rejects unlearned level");
    ok &= expect(!jrc::pet_buff::skill_can_auto_buff(haste, 4101004, 31),
        "template rejects level above masterlevel");
    MockSkill heal;
    heal.effect = true;
    heal.hit = true;
    heal.alert2 = true;
    ok &= expect(!jrc::pet_buff::skill_can_auto_buff(heal, 2301002, 30),
        "template rejects Heal via override");
    ok &= expect(!jrc::pet_buff::skill_can_auto_buff(haste, 1004, 1),
        "template rejects mount id via is_mount_skill");

    constexpr int32_t SKILL = 1001003;
    AutoBuffInputs ready = { true, true, false, true, true, true };

    Controller pending;
    pending.set_buff_skill(0, SKILL);
    ok &= expect(pending.decide_buff(0, ready), "buff cast admitted");
    ok &= expect(pending.has_pending_buff(), "cast marked pending");
    ok &= expect(pending.get_pending_skill() == SKILL,
        "pending skill exposed");
    ok &= expect(pending.get_pending_frames() ==
        Controller::BUFF_CONFIRM_TIMEOUT_FRAMES, "pending timeout exposed");
    pending.confirm_buff(SKILL);
    ok &= expect(!pending.has_pending_buff(), "confirm clears pending");
    ok &= expect(pending.get_buff_backoff(0) == 0 &&
        pending.get_buff_failures(0) == 0, "confirm clears backoff/failures");

    Controller gates;
    gates.set_buff_skill(0, SKILL);
    AutoBuffInputs no_mp = ready;
    no_mp.mp_ok = false;
    ok &= expect(!gates.decide_buff(0, no_mp), "MP shortage rejected");
    AutoBuffInputs no_cast = ready;
    no_cast.castable = false;
    ok &= expect(!gates.decide_buff(0, no_cast), "non-castable rejected");
    ok &= expect(gates.decide_buff(0, ready),
        "rejected gates do not consume cooldown");

    Controller timeout;
    timeout.set_buff_skill(0, SKILL);
    ok &= expect(timeout.decide_buff(0, ready), "timeout cast admitted");
    for (uint16_t frame = 0; frame < Controller::BUFF_CONFIRM_TIMEOUT_FRAMES; ++frame)
        timeout.tick();
    ok &= expect(!timeout.has_pending_buff(), "timeout clears pending");
    ok &= expect(timeout.get_buff_failures(0) == 1, "timeout counts a failure");
    ok &= expect(timeout.get_buff_backoff(0) ==
        Controller::BUFF_FAILURE_BACKOFF_FRAMES, "timeout enters backoff");
    ok &= expect(!timeout.decide_buff(0, ready), "backoff blocks recast");

    Controller failures;
    failures.set_buff_skill(0, SKILL);
    for (uint8_t attempt = 0; attempt < Controller::BUFF_MAX_FAILURES; ++attempt)
    {
        if (attempt > 0)
        {
            for (uint16_t frame = 0;
                frame < Controller::BUFF_FAILURE_BACKOFF_FRAMES; ++frame)
                failures.tick();
        }
        ok &= expect(failures.decide_buff(0, ready), "retry admitted");
        for (uint16_t frame = 0;
            frame < Controller::BUFF_CONFIRM_TIMEOUT_FRAMES; ++frame)
            failures.tick();
    }
    ok &= expect(failures.get_buff_failures(0) ==
        Controller::BUFF_MAX_FAILURES, "failure count reaches limit");
    ok &= expect(failures.is_buff_unconfirmed(0), "session unconfirmed");
    for (uint16_t frame = 0;
        frame < Controller::BUFF_FAILURE_BACKOFF_FRAMES; ++frame)
        failures.tick();
    ok &= expect(!failures.decide_buff(0, ready),
        "unconfirmed slot stays blocked for the session");

    failures.set_hp_threshold(33);
    failures.set_mp_threshold(44);
    failures.set_hp_item(2000000);
    failures.set_food_item(5000000);
    failures.reset_transient();
    ok &= expect(!failures.is_buff_unconfirmed(0) &&
        failures.get_buff_failures(0) == 0 &&
        failures.get_buff_backoff(0) == 0 &&
        !failures.has_pending_buff(), "reset clears transient state");
    ok &= expect(failures.get_hp_threshold() == 33 &&
        failures.get_mp_threshold() == 44 &&
        failures.get_hp_item() == 2000000 &&
        failures.get_food_item() == 5000000 &&
        failures.get_buff_skill(0) == SKILL, "reset preserves settings");

    using jrc::Buff;
    namespace Buffstat = jrc::Buffstat;
    using jrc::buff_is_active;
    Buff finite{ Buffstat::STUN, 0, 99, 5000 };
    ok &= expect(finite.remaining_ms == 5000, "finite buff counts down");
    ok &= expect(buff_is_active(finite), "value 0 finite buff is active");
    Buff infinite{ Buffstat::STUN, 0, 99, 0 };
    ok &= expect(infinite.remaining_ms == -1, "zero duration is infinite");
    ok &= expect(buff_is_active(infinite), "infinite buff is active");
    Buff cancelled{ Buffstat::STUN, 0, 99, 1000 };
    cancelled.remaining_ms = 0;
    ok &= expect(!buff_is_active(cancelled), "zero remaining is inactive");

    BuffSkillSlots duplicates{};
    duplicates[0] = 100;
    duplicates[1] = 100;
    duplicates[2] = 200;
    duplicates[3] = -1;
    duplicates[5] = 200;
    const BuffSkillSlots clean = sanitize_buff_skills(duplicates);
    ok &= expect(clean[0] == 100 && clean[1] == 0 &&
        clean[2] == 200 && clean[5] == 0,
        "sanitize keeps first occurrence");
    ok &= expect(buffs_run_before_hungry_exit(),
        "buffs run before hungry exit");

    if (ok)
        std::cout << "pet-buff-policy: all checks passed\n";
    return ok ? 0 : 1;
}
