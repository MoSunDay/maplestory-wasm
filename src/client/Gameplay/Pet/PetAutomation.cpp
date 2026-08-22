#include "PetAutomation.h"

#include <algorithm>

namespace jrc::pet_automation
{
    uint8_t clamp_threshold(int32_t value)
    {
        return static_cast<uint8_t>(std::clamp(value, 1, 99));
    }

    bool at_or_below_threshold(int32_t current, int32_t maximum, uint8_t threshold)
    {
        if (maximum <= 0 || current < 0)
            return false;
        return static_cast<int64_t>(current) * 100 <=
            static_cast<int64_t>(maximum) * clamp_threshold(threshold);
    }

    Controller::Controller(uint8_t hp_value, uint8_t mp_value)
        : hp_threshold(clamp_threshold(hp_value)),
          mp_threshold(clamp_threshold(mp_value))
    {
    }

    void Controller::tick()
    {
        if (hp_cooldown > 0)
            --hp_cooldown;
        if (mp_cooldown > 0)
            --mp_cooldown;
    }

    void Controller::set_hp_item(int32_t item_id) { hp_item = std::max(item_id, 0); }
    void Controller::set_mp_item(int32_t item_id) { mp_item = std::max(item_id, 0); }
    void Controller::set_hp_threshold(uint8_t value) { hp_threshold = clamp_threshold(value); }
    void Controller::set_mp_threshold(uint8_t value) { mp_threshold = clamp_threshold(value); }
    int32_t Controller::get_hp_item() const { return hp_item; }
    int32_t Controller::get_mp_item() const { return mp_item; }
    uint8_t Controller::get_hp_threshold() const { return hp_threshold; }
    uint8_t Controller::get_mp_threshold() const { return mp_threshold; }

    AutoPotDecisions Controller::decide(const AutoPotInputs& inputs)
    {
        AutoPotDecisions result;
        if (!inputs.alive || !inputs.pet_active)
            return result;

        bool use_hp = hp_item > 0 && hp_cooldown == 0 && inputs.hp_ability &&
            inputs.hp_item_available &&
            at_or_below_threshold(inputs.hp, inputs.max_hp, hp_threshold);
        bool use_mp = mp_item > 0 && mp_cooldown == 0 && inputs.mp_ability &&
            inputs.mp_item_available &&
            at_or_below_threshold(inputs.mp, inputs.max_mp, mp_threshold);

        if (use_hp && use_mp && hp_item == mp_item)
        {
            result.requests[0] = { Resource::BOTH, hp_item };
            result.count = 1;
            hp_cooldown = POTION_COOLDOWN_FRAMES;
            mp_cooldown = POTION_COOLDOWN_FRAMES;
            return result;
        }
        if (use_hp)
        {
            result.requests[result.count++] = { Resource::HP, hp_item };
            hp_cooldown = POTION_COOLDOWN_FRAMES;
            return result;
        }
        if (use_mp)
        {
            result.requests[result.count++] = { Resource::MP, mp_item };
            mp_cooldown = POTION_COOLDOWN_FRAMES;
        }
        return result;
    }
}
