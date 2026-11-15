#include "PetBuffPolicy.h"

#include <algorithm>

namespace jrc::pet_buff
{
    namespace
    {
        // Audit 2026-11-15 against link_repos/MapleStory-Server
        // src/client/SkillFactory.java loadFromData: ids whose explicit
        // switch verdict contradicts the client NX heuristic
        // (skillType == 2, else effect && !hit && !ball || action/0 ==
        // "alert2") were cross-checked with scripts/nxdump/nxdump2 on
        // assets/Skill.nx. Only ids that EXIST in Skill.nx are listed:
        // a skill without a client node can never be learned or assigned.
        // The remaining server-only ids (e.g. SHADOW_WEB 4111003/14111001,
        // AXE_BOOSTER 1101005, Evan skills) have no NX node and are omitted.

        // isBuff = false overrides; the NX shape looks like a buff.
        constexpr int32_t NONBUFF_IDS[] = {
            // Hero.MONSTER_MAGNET 1121001: server :181 pulls, has effect.
            1121001,
            // Cleric.HEAL 2301002: server :173 effect+hit+alert2 shape.
            2301002,
            // Ranger.MORTAL_BLOW 3110001: server :174 effect, no hit.
            3110001,
            // Sniper.MORTAL_BLOW 3210001: server :175 effect, no hit.
            3210001,
            // Bandit.STEAL 4201004: server :178 effect, no hit.
            4201004,
            // ChiefBandit.CHAKRA 4211001: server :192 effect+alert shape.
            4211001,
            // Shadower.SMOKE_SCREEN 4221006: server :179 effect, no hit.
            4221006,
            // SuperGM.HEAL_PLUS_DISPEL 9101000: server :180 GM skill.
            9101000,
        };

        // isBuff = true overrides; the NX shape looks like a plain/attack
        // skill, but the server applies a player buff the pet can confirm.
        // Attack/proc skills without statups (FINAL_ATTACK 11101002/13101002,
        // ENERGY_CHARGE 15100004, ENERGY_DRAIN 15111001, SPARK 15111006) are
        // deliberately excluded: the server sends GIVE_BUFF only when
        // localstatups is non-empty (MapleStatEffect.java :1104-1108), so a
        // pet cast could never be confirmed and would loop on backoff.
        constexpr int32_t BUFF_IDS[] = {
            // Swordsman.IRON_BODY 1001003: server :201, no effect node.
            1001003,
            // DarkKnight.AURA_OF_BEHOLDER 1320008: server :234.
            1320008,
            // DarkKnight.HEX_OF_BEHOLDER 1320009: server :237.
            1320009,
            // Marauder.TRANSFORMATION 5111005: server :301.
            5111005,
            // Buccaneer.SUPER_TRANSFORMATION 5121003: server :302.
            5121003,
            // Noblesse.BLESSING_OF_THE_FAIRY 10000012: server :310.
            10000012,
            // WindArcher.EAGLE_EYE 13111005: server :333.
            13111005,
            // ThunderBreaker.TRANSFORMATION 15111002: server :353.
            15111002,
            // Legend.BLESSING_OF_THE_FAIRY 20000012: server :354.
            20000012,
            // Aran.COMBO_ABILITY 21000000: server :368.
            21000000,
        };

        constexpr int32_t BOOSTER_IDS[] = {
            1101004, 1101005, 1201004, 1201005, 1301004, 1301005,
            2111005, 2211005, 3101002, 3201002, 4101003, 4201002,
            5101006, 5201003,
        };

        constexpr int32_t CHARGE_IDS[] = {
            1211003, 1211004, 1211005, 1211006, 1211007, 1211008,
            1221003, 1221004,
        };

        bool in_sorted_table(const int32_t* table, size_t size, int32_t id)
        {
            return std::binary_search(table, table + size, id);
        }

        constexpr bool sorted(const int32_t* table, size_t size)
        {
            for (size_t i = 1; i < size; ++i)
                if (table[i - 1] >= table[i])
                    return false;
            return true;
        }

        static_assert(sorted(NONBUFF_IDS, sizeof(NONBUFF_IDS)
            / sizeof(NONBUFF_IDS[0])), "non-buff table must stay sorted");
        static_assert(sorted(BUFF_IDS, sizeof(BUFF_IDS)
            / sizeof(BUFF_IDS[0])), "buff table must stay sorted");
        static_assert(sorted(BOOSTER_IDS, sizeof(BOOSTER_IDS)
            / sizeof(BOOSTER_IDS[0])), "booster table must stay sorted");
        static_assert(sorted(CHARGE_IDS, sizeof(CHARGE_IDS)
            / sizeof(CHARGE_IDS[0])), "charge table must stay sorted");
    }

    bool known_nonbuff_skill(int32_t id)
    {
        return in_sorted_table(NONBUFF_IDS, sizeof(NONBUFF_IDS)
            / sizeof(NONBUFF_IDS[0]), id);
    }

    bool known_buff_skill(int32_t id)
    {
        return in_sorted_table(BUFF_IDS, sizeof(BUFF_IDS)
            / sizeof(BUFF_IDS[0]), id);
    }

    bool is_booster_skill(int32_t id)
    {
        return in_sorted_table(BOOSTER_IDS, sizeof(BOOSTER_IDS)
            / sizeof(BOOSTER_IDS[0]), id);
    }

    bool is_charge_skill(int32_t id)
    {
        return in_sorted_table(CHARGE_IDS, sizeof(CHARGE_IDS)
            / sizeof(CHARGE_IDS[0]), id);
    }

    bool buff_slot_satisfied(int32_t skill_id, bool has_skill_buff,
        bool booster_active, bool wk_charge_active)
    {
        return has_skill_buff
            || (booster_active && is_booster_skill(skill_id))
            || (wk_charge_active && is_charge_skill(skill_id));
    }

    bool pet_buff_skill_allowed(const PolicyInput& input)
    {
        if (input.level <= 0 || !input.level_data_available ||
            input.passive || input.attack || input.summon ||
            input.mount_skill)
            return false;

        const bool buff = input.skill_type != -1
            ? input.skill_type == 2
            : (input.has_effect && !input.has_hit && !input.has_ball) ||
              input.alert2_action;

        if (known_nonbuff_skill(input.skill_id))
            return false;
        if (known_buff_skill(input.skill_id))
            return true;
        return buff;
    }
}
