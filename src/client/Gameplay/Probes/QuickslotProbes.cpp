#include "../../IO/UI.h"
#include "../../IO/UITypes/UIStatusBar.h"
#include "../../IO/UITypes/UIItemInventory.h"
#include "../Stage.h"

#include <cstdint>

extern "C"
{
    int msiteminventory_icon_item_id(int slot)
    {
        auto inventory = jrc::UI::get().get_element<jrc::UIItemInventory>();
        if (!inventory)
        {
            return -1;
        }

        return static_cast<int>(inventory->item_id_at(
            static_cast<int16_t>(slot)));
    }

    int msquickslot_skill(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_skill(
                static_cast<int16_t>(index)))
            : -1;
    }

    int msquickslot_item(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_item(
                static_cast<int16_t>(index)))
            : -1;
    }

    int msquickslot_center_x(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_center(
                static_cast<int16_t>(index)).x())
            : -1;
    }

    int msquickslot_center_y(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_center(
                static_cast<int16_t>(index)).y())
            : -1;
    }

    int msquickslot_panel_left()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_panel_bounds().l())
            : -1;
    }

    int msquickslot_panel_top()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_panel_bounds().t())
            : -1;
    }

    int msquickslot_panel_right()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_panel_bounds().r())
            : -1;
    }

    int msquickslot_panel_bottom()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_panel_bounds().b())
            : -1;
    }

    int msquickslot_cooldown(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        if (!statusbar)
        {
            return -1;
        }

        const int32_t skill_id = statusbar->get_quickslot_skill(
            static_cast<int16_t>(index));
        return skill_id > 0
            ? jrc::Stage::get().get_player().get_cooldown_seconds(skill_id)
            : 0;
    }

    int msquickslot_cooldown_label(int index)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_cooldown_length(
                static_cast<int16_t>(index)))
            : -1;
    }

    int msquickslot_hovered()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->get_quickslot_hovered())
            : -2;
    }
}
