#include "../Stage.h"

#include "../../Character/Inventory/InventoryType.h"
#include "../../IO/Field/UIFieldClock.h"
#include "../../IO/CharacterInfo/UIUserInfo.h"
#include "../../IO/UI.h"
#include "../../IO/Trade/UIPlayerContext.h"
#include "../../IO/UITypes/UINpcTalk.h"
#include "../../IO/UITypes/Pet/UIPetOptions.h"
#include "../../IO/UITypes/UIShop.h"
#include "../../IO/UITypes/UIStatusBar.h"

#include <cstdint>
#include <optional>
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

    int msplayer_screen_x()
    {
        auto& stage = jrc::Stage::get();
        return stage.get_player().get_position().x() + stage.get_view_position().x();
    }

    int msplayer_screen_y()
    {
        auto& stage = jrc::Stage::get();
        return stage.get_player().get_position().y() + stage.get_view_position().y();
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

    int mschar_screen_x(int32_t character_id)
    {
        auto& stage = jrc::Stage::get();
        auto character = stage.get_chars().get_char(character_id);
        return character ? character->get_position().x() + stage.get_view_position().x() :
            MISSING_COORDINATE;
    }

    int mschar_screen_y(int32_t character_id)
    {
        auto& stage = jrc::Stage::get();
        auto character = stage.get_chars().get_char(character_id);
        return character ? character->get_position().y() + stage.get_view_position().y() :
            MISSING_COORDINATE;
    }

    int msmob_count()
    {
        return static_cast<int>(jrc::Stage::get().get_mobs().size());
    }

    int msnearest_mob_x()
    {
        auto& stage = jrc::Stage::get();
        auto position = stage.get_mobs().find_nearest_position(
            stage.get_player().get_position(), 100);
        return position ? position->x() : MISSING_COORDINATE;
    }

    int msnearest_mob_y()
    {
        auto& stage = jrc::Stage::get();
        auto position = stage.get_mobs().find_nearest_position(
            stage.get_player().get_position(), 100);
        return position ? position->y() : MISSING_COORDINATE;
    }

    int mspet_active()
    {
        return jrc::Stage::get().get_player().get_pet(0) ? 1 : 0;
    }

    int mspet_item_id()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? pet->get_itemid() : 0;
    }

    int mspet_capability_mask()
    {
        return jrc::Stage::get().get_pet_capabilities().mask();
    }

    int mspet_x()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? pet->get_position().x() : MISSING_COORDINATE;
    }

    int mspet_y()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? pet->get_position().y() : MISSING_COORDINATE;
    }

    int mspet_stance()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? static_cast<int>(pet->get_stance()) : -1;
    }

    int mspet_foothold_id()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? pet->get_foothold_id() : 0;
    }

    int mspet_onground()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet && pet->is_onground() ? 1 : 0;
    }

    int mspet_move_intent()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? static_cast<int>(pet->get_last_move_intent()) : -1;
    }

    int mspet_horizontal_collision()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? static_cast<int>(pet->get_last_horizontal_collision()) : -1;
    }

    int mspet_jump_trigger_count()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? static_cast<int>(pet->get_jump_trigger_count()) : 0;
    }

    int mspet_has_loot_target()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet && pet->has_loot_target() ? 1 : 0;
    }

    int mspet_confirmed_pickups()
    {
        return static_cast<int>(jrc::Stage::get().get_drops().pet_pickup_count());
    }

    int mspet_loot_target_oid()
    {
        return jrc::Stage::get().get_pet_loot_target_oid();
    }

    int mspet_last_warp_distance()
    {
        const auto* pet = jrc::Stage::get().get_player().get_pet(0);
        return pet ? pet->get_last_warp_owner_distance() : 0;
    }

    int msdrop_count()
    {
        return static_cast<int>(jrc::Stage::get().get_drops().size());
    }

    int msdrop_has(int32_t oid)
    {
        return jrc::Stage::get().get_drops().has_drop(oid) ? 1 : 0;
    }

    int msplayer_hp()
    {
        return jrc::Stage::get().get_player().get_stats().get_stat(jrc::Maplestat::HP);
    }

    int msplayer_max_hp()
    {
        return jrc::Stage::get().get_player().get_stats().get_stat(jrc::Maplestat::MAXHP);
    }

    int msplayer_mp()
    {
        return jrc::Stage::get().get_player().get_stats().get_stat(jrc::Maplestat::MP);
    }

    int msplayer_max_mp()
    {
        return jrc::Stage::get().get_player().get_stats().get_stat(jrc::Maplestat::MAXMP);
    }

    int msplayer_meso()
    {
        return static_cast<int>(jrc::Stage::get().get_player().get_inventory().get_meso());
    }

    int mspet_auto_hp_item()
    {
        return jrc::Stage::get().get_pet_automation().get_hp_item();
    }

    int mspet_auto_mp_item()
    {
        return jrc::Stage::get().get_pet_automation().get_mp_item();
    }

    int mspet_auto_hp_threshold()
    {
        return jrc::Stage::get().get_pet_automation().get_hp_threshold();
    }

    int mspet_auto_mp_threshold()
    {
        return jrc::Stage::get().get_pet_automation().get_mp_threshold();
    }

    int mspet_options_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::PETOPTIONS) ? 1 : 0;
    }

    int msuserinfo_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::USERINFO) ? 1 : 0;
    }

    int msuserinfo_target_id()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? window->target_id() : 0;
    }

    int msuserinfo_loaded()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window && window->info_loaded() ? 1 : 0;
    }

    int msuserinfo_pet_count()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? static_cast<int>(window->pet_count()) : 0;
    }

    int msuserinfo_pet_expanded()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window && window->pet_expanded() ? 1 : 0;
    }

    int msuserinfo_x()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? window->origin().x() : MISSING_COORDINATE;
    }

    int msuserinfo_y()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? window->origin().y() : MISSING_COORDINATE;
    }

    int msuserinfo_width()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? window->size().x() : 0;
    }

    int msuserinfo_height()
    {
        auto window = jrc::UI::get().get_element<jrc::UIUserInfo>();
        return window ? window->size().y() : 0;
    }

    int msplayer_context_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::PLAYER_CONTEXT) ? 1 : 0;
    }

    int mspet_options_hp_item()
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options ? options->selected_hp_item() : -1;
    }

    int mspet_options_mp_item()
    {
        auto options = jrc::UI::get().get_element<jrc::UIPetOptions>();
        return options ? options->selected_mp_item() : -1;
    }

    int mspet_options_menu_x()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(jrc::StatusBarPopup::Action::PET_OPTIONS)
            : std::nullopt;
        return point ? point->x() : MISSING_COORDINATE;
    }

    int mspet_options_menu_y()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(jrc::StatusBarPopup::Action::PET_OPTIONS)
            : std::nullopt;
        return point ? point->y() : MISSING_COORDINATE;
    }

    int msitems_menu_x()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(jrc::StatusBarPopup::Action::ITEMS)
            : std::nullopt;
        return point ? point->x() : MISSING_COORDINATE;
    }

    int msitems_menu_y()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(jrc::StatusBarPopup::Action::ITEMS)
            : std::nullopt;
        return point ? point->y() : MISSING_COORDINATE;
    }

    int msshop_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::SHOP) ? 1 : 0;
    }

    int msshop_buy_item(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_buy_item(row) : 0;
    }

    int msshop_buy_x(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_buy_center(row).x() : MISSING_COORDINATE;
    }

    int msshop_buy_y(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_buy_center(row).y() : MISSING_COORDINATE;
    }

    int msshop_sell_item(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_sell_item(row) : 0;
    }

    int msshop_sell_x(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_sell_center(row).x() : MISSING_COORDINATE;
    }

    int msshop_sell_y(int16_t row)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->visible_sell_center(row).y() : MISSING_COORDINATE;
    }

    int msshop_selected()
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop ? shop->sell_selection() : MISSING_COORDINATE;
    }

    // id follows UIShop::Buttons: 0=BUY_ITEM, 2=EXIT, 23=SELL_ALL
    // (1 reserved: SELL_ITEM removed; double-click a row to sell one item).
    int msshop_button_x(int16_t id)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop
            ? shop->button_center(static_cast<uint16_t>(id)).x()
            : MISSING_COORDINATE;
    }

    int msshop_button_y(int16_t id)
    {
        auto shop = jrc::UI::get().get_element<jrc::UIShop>();
        return shop
            ? shop->button_center(static_cast<uint16_t>(id)).y()
            : MISSING_COORDINATE;
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
