#include "../Stage.h"

#include "../../Character/Inventory/InventoryType.h"
#include "../../IO/Field/UIFieldClock.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UINpcTalk.h"

#include <cstdint>
#include <string>

namespace
{
    constexpr int MISSING_COORDINATE = -32768;

    int item_quantity(int32_t item_id)
    {
        const auto type = jrc::InventoryType::by_item_id(item_id);
        if (type == jrc::InventoryType::NONE)
        {
            return 0;
        }

        const auto& inventory = jrc::Stage::get().get_player().get_inventory();
        const int16_t slot_max = inventory.get_slotmax(type);
        int quantity = 0;
        for (int16_t slot = 1; slot <= slot_max; ++slot)
        {
            if (inventory.get_item_id(type, slot) == item_id)
            {
                quantity += inventory.get_item_count(type, slot);
            }
        }
        return quantity;
    }
}

extern "C"
{
    int msmap_id()
    {
        return jrc::Stage::get().get_mapid();
    }

    int msplayer_x()
    {
        return jrc::Stage::get().get_player().get_position().x();
    }

    int msplayer_y()
    {
        return jrc::Stage::get().get_player().get_position().y();
    }

    int msother_character_count()
    {
        return static_cast<int>(jrc::Stage::get().get_chars().get_chars()->size());
    }

    int mschar_visible(int32_t character_id)
    {
        return jrc::Stage::get().get_chars().get_char(character_id) ? 1 : 0;
    }

    int mschar_x(int32_t character_id)
    {
        auto character = jrc::Stage::get().get_chars().get_char(character_id);
        return character ? character->get_position().x() : MISSING_COORDINATE;
    }

    int mschar_y(int32_t character_id)
    {
        auto character = jrc::Stage::get().get_chars().get_char(character_id);
        return character ? character->get_position().y() : MISSING_COORDINATE;
    }

    int msmob_count()
    {
        return static_cast<int>(jrc::Stage::get().get_mobs().size());
    }

    int msinventory_quantity(int32_t item_id)
    {
        return item_quantity(item_id);
    }

    int msinventory_slot(int32_t item_id)
    {
        const auto type = jrc::InventoryType::by_item_id(item_id);
        return type == jrc::InventoryType::NONE
            ? 0
            : jrc::Stage::get().get_player().get_inventory().find_item(type, item_id);
    }

    int msnpc_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::NPCTALK) ? 1 : 0;
    }

    const char* msnpc_prompt()
    {
        auto dialogue = jrc::UI::get().get_element<jrc::UINpcTalk>();
        return dialogue ? dialogue->get_prompt_text().c_str() : "";
    }

    int msfield_clock_seconds()
    {
        auto clock = jrc::UI::get().get_element<jrc::UIFieldClock>();
        return clock ? clock->remaining_seconds() : 0;
    }

    int msfield_clock_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::FIELDCLOCK) ? 1 : 0;
    }

    int msfield_object_active(const char* name)
    {
        if (!name)
        {
            return 0;
        }
        return static_cast<int>(jrc::Stage::get().count_active_named_objects(name));
    }
}
