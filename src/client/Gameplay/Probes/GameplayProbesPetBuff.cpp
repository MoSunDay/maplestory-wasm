// Pet auto-buff probes, split from GameplayProbes.cpp to respect the
// per-file line budget.
#include "../Stage.h"

#include "../Pet/PetAutomation.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/Pet/UIPetOptions.h"

#include <cstddef>
#include <cstdint>

namespace
{
    constexpr int MISSING_COORDINATE = -32768;

    bool valid_buff_slot(int slot)
    {
        return slot >= 0 &&
            slot < static_cast<int>(jrc::pet_automation::MAX_BUFF_SKILLS);
    }
}

extern "C"
{
    // Skill id stored in an auto-buff slot, or 0 when the slot is empty.
    int mspet_auto_buff_skill(int slot)
    {
        if (slot < 0)
            return 0;
        return jrc::Stage::get().get_pet_automation().get_buff_skill(
            static_cast<size_t>(slot));
    }

    // 1 while the player has an active buff from the given skill, 0
    // otherwise (including no buff and invalid skill ids).
    int mspet_buff_active(int skill_id)
    {
        return skill_id > 0 &&
            jrc::Stage::get().get_player().has_skill_buff(skill_id) ? 1 : 0;
    }

    // Number of auto-buff slots that currently hold a skill.
    int mspet_auto_buff_count()
    {
        return static_cast<int>(
            jrc::Stage::get().get_pet_automation().get_buff_skill_count());
    }

    // Logical screen center x of an auto-buff slot in the pet options panel,
    // or -32768 when the panel is closed or the slot is invalid.
    int mspet_options_buff_slot_x(int slot)
    {
        if (!valid_buff_slot(slot))
            return MISSING_COORDINATE;
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->buff_slot_center(static_cast<size_t>(slot)).x()
            : MISSING_COORDINATE;
    }

    // Logical screen center y of an auto-buff slot in the pet options panel,
    // or -32768 when the panel is closed or the slot is invalid.
    int mspet_options_buff_slot_y(int slot)
    {
        if (!valid_buff_slot(slot))
            return MISSING_COORDINATE;
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->buff_slot_center(static_cast<size_t>(slot)).y()
            : MISSING_COORDINATE;
    }
}
