#include "PetSettingsStore.h"

#include "PetAutomation.h"

#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace jrc::pet_settings
{
    namespace
    {
        constexpr const char* HP_KEY = "maplestory.pet.auto_hp_threshold";
        constexpr const char* MP_KEY = "maplestory.pet.auto_mp_threshold";
        constexpr const char* FOOD_KEY = "maplestory.pet.auto_food_item";
        constexpr const char* BUFF_KEY_PREFIX = "maplestory.pet.auto_buff_";

        int32_t load_value(const char* key, int32_t fallback)
        {
#ifdef __EMSCRIPTEN__
            return EM_ASM_INT({
                try {
                    const value = window.localStorage.getItem(UTF8ToString($0));
                    if (value === null) return $1;
                    const number = Number.parseInt(value, 10);
                    return Number.isFinite(number) ? number : $1;
                } catch (_) {
                    return $1;
                }
            }, key, fallback);
#else
            (void) key;
            return fallback;
#endif
        }

        std::string buff_key(size_t slot)
        {
            return std::string(BUFF_KEY_PREFIX) + std::to_string(slot);
        }

        bool save_value(const char* key, int32_t value)
        {
#ifdef __EMSCRIPTEN__
            return EM_ASM_INT({
                try {
                    window.localStorage.setItem(UTF8ToString($0), String($1));
                    return 1;
                } catch (_) {
                    return 0;
                }
            }, key, value) != 0;
#else
            (void) key;
            (void) value;
            return true;
#endif
        }
    }

    Thresholds load()
    {
        return {
            pet_automation::clamp_threshold(load_value(HP_KEY, 50)),
            pet_automation::clamp_threshold(load_value(MP_KEY, 50))
        };
    }

    bool save(const Thresholds& thresholds)
    {
        uint8_t hp = pet_automation::clamp_threshold(thresholds.hp);
        uint8_t mp = pet_automation::clamp_threshold(thresholds.mp);
        return save_value(HP_KEY, hp) && save_value(MP_KEY, mp);
    }

    int32_t load_food_item()
    {
        return load_value(FOOD_KEY, 0);
    }

    bool save_food_item(int32_t item_id)
    {
        return save_value(FOOD_KEY, item_id > 0 ? item_id : 0);
    }

    pet_automation::BuffSkillSlots load_buff_skills()
    {
        pet_automation::BuffSkillSlots skills{};
        for (size_t slot = 0; slot < skills.size(); ++slot)
        {
            std::string key = buff_key(slot);
            int32_t value = load_value(key.c_str(), 0);
            skills[slot] = value > 0 ? value : 0;
        }
        return skills;
    }

    bool save_buff_skill(size_t slot, int32_t skill_id)
    {
        if (slot >= pet_automation::MAX_BUFF_SKILLS)
            return false;

        std::string key = buff_key(slot);
        return save_value(key.c_str(), skill_id > 0 ? skill_id : 0);
    }
}
