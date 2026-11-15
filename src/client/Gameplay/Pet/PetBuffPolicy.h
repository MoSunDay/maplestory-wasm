#pragma once

#include <cstdint>

namespace jrc::pet_buff
{
    // Skill properties relevant to pet auto-buff admission.
    // The FIRST FIVE fields are load-bearing: callers aggregate-initialise
    // PolicyInput positionally, so their order and meaning must not change.
    struct PolicyInput
    {
        int32_t level = 0;
        bool passive = false;
        bool attack = false;
        bool summon = false;
        bool level_data_available = true;

        int32_t skill_id = 0;
        // Raw NX "skillType" value; -1 when the node is absent.
        int32_t skill_type = -1;
        bool has_effect = false;
        bool has_hit = false;
        bool has_ball = false;
        // action/0 == "alert2", the buff cast pose.
        bool alert2_action = false;
        bool mount_skill = false;
    };

    // Monster Rider variants. The server's GIVE_BUFF carries the MOUNT item
    // id, so the client can never confirm a cast and would loop on backoff.
    constexpr bool is_mount_skill(int32_t id)
    {
        return id == 1004 || id == 10001004 || id == 20001004
            || id == 20011004;
    }

    // Explicit overrides from the server's SkillFactory.loadFromData id
    // switches, for skills present in Skill.nx whose NX shape disagrees with
    // the server verdict. See PetBuffPolicy.cpp for the audit table.
    bool known_nonbuff_skill(int32_t id);
    bool known_buff_skill(int32_t id);

    // A pet may only auto-cast learned, non-passive, non-attack, non-summon
    // skills whose level data exists and which the server treats as a buff
    // (skillType == 2, else the effect/hit/ball + alert2 heuristic, else the
    // explicit override lists). Recovery time is deliberately not part of
    // the decision.
    bool pet_buff_skill_allowed(const PolicyInput& input);

    template <typename SkillLike>
    bool skill_can_auto_buff(const SkillLike& skill, int32_t skill_id,
        int32_t level)
    {
        return pet_buff_skill_allowed({
            level,
            skill.is_passive(),
            skill.is_attack(),
            skill.is_summon(),
            level > 0 && level <= skill.get_masterlevel(),
            skill_id,
            skill.get_skill_type(),
            skill.has_effect(),
            skill.has_hit(),
            skill.has_ball(),
            skill.has_alert2_action(),
            is_mount_skill(skill_id)
        });
    }

    // Skill families the server multiplexes onto ONE Buffstat each:
    // every booster shares Buffstat::BOOSTER, every elemental charge shares
    // Buffstat::WK_CHARGE.
    bool is_booster_skill(int32_t id);
    bool is_charge_skill(int32_t id);

    // A slot is satisfied when its own skill buff is active, or — for the
    // stats the server multiplexes — when ANY skill on the same stat is.
    bool buff_slot_satisfied(int32_t skill_id, bool has_skill_buff,
        bool booster_active, bool wk_charge_active);
}
