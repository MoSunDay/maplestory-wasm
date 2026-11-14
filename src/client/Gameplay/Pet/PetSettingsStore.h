#pragma once

#include "PetAutomation.h"

#include <cstdint>

namespace jrc::pet_settings
{
    struct Thresholds
    {
        uint8_t hp = 50;
        uint8_t mp = 50;
    };

    Thresholds load();
    bool save(const Thresholds& thresholds);

    // Selected pet-food item id (0 = none). Feed choices have no server
    // keymap entry, so they live in browser storage like the thresholds.
    int32_t load_food_item();
    bool save_food_item(int32_t item_id);

    // Auto-buff skill slots (0 = empty). Each slot is stored under its own
    // browser-storage key so a corrupt entry cannot invalidate the others.
    pet_automation::BuffSkillSlots load_buff_skills();
    bool save_buff_skill(size_t slot, int32_t skill_id);
}
