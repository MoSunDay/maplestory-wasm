#include "PetSettingsStore.h"

#include "PetAutomation.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace jrc::pet_settings
{
    namespace
    {
        constexpr const char* HP_KEY = "maplestory.pet.auto_hp_threshold";
        constexpr const char* MP_KEY = "maplestory.pet.auto_mp_threshold";

        int32_t load_value(const char* key)
        {
#ifdef __EMSCRIPTEN__
            return EM_ASM_INT({
                try {
                    const value = window.localStorage.getItem(UTF8ToString($0));
                    if (value === null) return 50;
                    const number = Number.parseInt(value, 10);
                    return Number.isFinite(number) ? number : 50;
                } catch (_) {
                    return 50;
                }
            }, key);
#else
            (void) key;
            return 50;
#endif
        }

        bool save_value(const char* key, uint8_t value)
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
            pet_automation::clamp_threshold(load_value(HP_KEY)),
            pet_automation::clamp_threshold(load_value(MP_KEY))
        };
    }

    bool save(const Thresholds& thresholds)
    {
        uint8_t hp = pet_automation::clamp_threshold(thresholds.hp);
        uint8_t mp = pet_automation::clamp_threshold(thresholds.mp);
        return save_value(HP_KEY, hp) && save_value(MP_KEY, mp);
    }
}
