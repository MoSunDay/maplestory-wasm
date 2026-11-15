// Skill book geometry probes, split from GameplayProbes.cpp to respect the
// per-file line budget.
#include "../../IO/UI.h"
#include "../../IO/UITypes/UISkillBook.h"

#include <cstdint>

namespace
{
    constexpr int MISSING_COORDINATE = -32768;
    constexpr int SKILLBOOK_TABS = 5;
    constexpr int SKILLBOOK_ROWS = 4;

    jrc::UISkillbook* active_skillbook()
    {
        auto skillbook = jrc::UI::get().get_element<jrc::UISkillbook>();
        return skillbook && skillbook->is_active() ? skillbook.get() : nullptr;
    }

    bool valid_tab(int tab)
    {
        return tab >= 0 && tab < SKILLBOOK_TABS;
    }

    bool valid_row(int row)
    {
        return row >= 0 && row < SKILLBOOK_ROWS;
    }

    bool row_has_skill(const jrc::UISkillbook& skillbook, int row)
    {
        return valid_row(row) &&
            skillbook.visible_skill_id(static_cast<uint16_t>(row)) != 0;
    }
}

extern "C"
{
    // Logical screen center x of a skill book tab button, or -32768 when
    // the book is closed or the tab index is invalid.
    int msskillbook_tab_x(int tab)
    {
        auto skillbook = active_skillbook();
        return skillbook && valid_tab(tab)
            ? skillbook->tab_button_center(static_cast<uint16_t>(tab)).x()
            : MISSING_COORDINATE;
    }

    // Logical screen center y of a skill book tab button, or -32768 when
    // the book is closed or the tab index is invalid.
    int msskillbook_tab_y(int tab)
    {
        auto skillbook = active_skillbook();
        return skillbook && valid_tab(tab)
            ? skillbook->tab_button_center(static_cast<uint16_t>(tab)).y()
            : MISSING_COORDINATE;
    }

    // Skill id on a visible skill book row, or 0 when the book is closed,
    // the row is invalid, or the row is empty.
    int msskillbook_row_skill(int row)
    {
        auto skillbook = active_skillbook();
        return skillbook && valid_row(row)
            ? skillbook->visible_skill_id(static_cast<uint16_t>(row))
            : 0;
    }

    // Logical screen center x of a visible skill icon's drag hit box, or
    // -32768 when the book is closed or the row is invalid/empty.
    int msskillbook_icon_x(int row)
    {
        auto skillbook = active_skillbook();
        return skillbook && row_has_skill(*skillbook, row)
            ? skillbook->visible_icon_center(static_cast<uint16_t>(row)).x()
            : MISSING_COORDINATE;
    }

    // Logical screen center y of a visible skill icon's drag hit box, or
    // -32768 when the book is closed or the row is invalid/empty.
    int msskillbook_icon_y(int row)
    {
        auto skillbook = active_skillbook();
        return skillbook && row_has_skill(*skillbook, row)
            ? skillbook->visible_icon_center(static_cast<uint16_t>(row)).y()
            : MISSING_COORDINATE;
    }

    // Whether the skill book element exists at all (0 or 1).
    int msskillbook_exists()
    {
        return jrc::UI::get().has_element(jrc::UIElement::SKILLBOOK) ? 1 : 0;
    }

    // Skill id currently selected in the open skill book, or 0 when the
    // book is closed or nothing is selected.
    int msskillbook_selected()
    {
        auto skillbook = jrc::UI::get().get_element<jrc::UISkillbook>();
        return skillbook && skillbook->is_active()
            ? skillbook->get_selected_skill()
            : 0;
    }

    // Whether the skill book element is active (0 or 1).
    int msskillbook_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::SKILLBOOK) ? 1 : 0;
    }
}
