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

    // Draft skill id in an auto-buff slot of the open pet options panel, or
    // -1 when the panel is closed or the slot is invalid. Never reads the
    // live controller, so it exposes edits before OK persists them.
    int mspet_options_buff_skill(int slot)
    {
        if (!valid_buff_slot(slot))
            return -1;
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->draft_buff_skill(static_cast<size_t>(slot))
            : -1;
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

    // Screen x that the HP (hp != 0) or MP slider maps back to the given
    // 1..99 percent, or -32768 when the panel is closed.
    int mspet_options_slider_x(int hp, int percent)
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->slider_x_for_percent(hp != 0, percent)
            : MISSING_COORDINATE;
    }

    // Logical screen center y of the HP (hp != 0) or MP slider in the pet
    // options panel, or -32768 when the panel is closed.
    int mspet_options_slider_y(int hp)
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->slider_center(hp != 0).y()
            : MISSING_COORDINATE;
    }

    // Logical screen center x of the HP/MP potion selector in the pet
    // options panel, or -32768 when the panel is closed. The selector has a
    // single row per resource, so next only exists for probe symmetry.
    int mspet_options_potion_x(int hp, int next)
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->potion_center(hp != 0, next != 0).x()
            : MISSING_COORDINATE;
    }

    // Logical screen center y of the HP/MP potion selector in the pet
    // options panel, or -32768 when the panel is closed.
    int mspet_options_potion_y(int hp, int next)
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->potion_center(hp != 0, next != 0).y()
            : MISSING_COORDINATE;
    }

    // Logical screen center x of the OK button in the pet options panel, or
    // -32768 when the panel is closed.
    int mspet_options_ok_x()
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->ok_center().x()
            : MISSING_COORDINATE;
    }

    // Logical screen center y of the OK button in the pet options panel, or
    // -32768 when the panel is closed.
    int mspet_options_ok_y()
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options && options->is_active()
            ? options->ok_center().y()
            : MISSING_COORDINATE;
    }
}
