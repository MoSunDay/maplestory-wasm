#include "../Stage.h"

#include "../../Constants.h"
#include "../../Character/Inventory/InventoryType.h"
#include "../../IO/Field/UIFieldClock.h"
#include "../../IO/CharacterInfo/UIUserInfo.h"
#include "../../IO/UI.h"
#include "../../IO/Trade/UIPlayerContext.h"
#include "../../IO/UITypes/UINpcTalk.h"
#include "../../IO/UITypes/Pet/UIPetOptions.h"
#include "../../IO/UITypes/Quest/UIQuestLog.h"
#include "../../IO/UITypes/Settings/UIGameOptions.h"
#include "../../IO/UITypes/UIShop.h"
#include "../../IO/UITypes/UIItemInventory.h"
#include "../../IO/UITypes/UISkillBook.h"
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
    int msui_resolution_width()
    {
        return jrc::Constants::viewwidth();
    }

    int msui_resolution_height()
    {
        return jrc::Constants::viewheight();
    }

    int msgame_options_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::GAMEOPTIONS) ? 1 : 0;
    }

    int msgame_options_index()
    {
        auto options = jrc::UI::get().get_element<jrc::UIGameOptions>();
        return options ? static_cast<int>(options->selected_index()) : -1;
    }

    int msgame_options_next_x()
    {
        auto options = jrc::UI::get().get_element<jrc::UIGameOptions>();
        return options ? options->next_center().x() : MISSING_COORDINATE;
    }

    int msgame_options_next_y()
    {
        auto options = jrc::UI::get().get_element<jrc::UIGameOptions>();
        return options ? options->next_center().y() : MISSING_COORDINATE;
    }

    int msgame_options_ok_x()
    {
        auto options = jrc::UI::get().get_element<jrc::UIGameOptions>();
        return options ? options->ok_center().x() : MISSING_COORDINATE;
    }

    int msgame_options_ok_y()
    {
        auto options = jrc::UI::get().get_element<jrc::UIGameOptions>();
        return options ? options->ok_center().y() : MISSING_COORDINATE;
    }

    int msgame_options_menu_x()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(
                jrc::StatusBarPopup::Action::GAME_OPTIONS)
            : std::nullopt;
        return point ? point->x() : MISSING_COORDINATE;
    }

    int msgame_options_menu_y()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        auto point = statusbar
            ? statusbar->popup_action_center(
                jrc::StatusBarPopup::Action::GAME_OPTIONS)
            : std::nullopt;
        return point ? point->y() : MISSING_COORDINATE;
    }

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

    int msplayer_expression()
    {
        return static_cast<int>(
            jrc::Stage::get().get_player().get_look().get_expression());
    }

    int mschar_expression(int32_t character_id)
    {
        auto character = jrc::Stage::get().get_chars().get_char(character_id);
        return character ? static_cast<int>(
            character->get_look().get_expression()) : -1;
    }

    int mschar_stance(int32_t character_id)
    {
        auto character = jrc::Stage::get().get_chars().get_char(character_id);
        return character ? static_cast<int>(
            character->get_look().get_stance()) : -1;
    }

    int mschar_frame(int32_t character_id)
    {
        auto character = jrc::Stage::get().get_chars().get_char(character_id);
        return character ? character->get_look().get_frame() : -1;
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

    int msmob_state(int index, int field)
    {
        if (index < 0)
        {
            return -1;
        }
        return jrc::Stage::get().get_mobs().probe_state(
            static_cast<size_t>(index), field);
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

    int msdrop_nearest_x()
    {
        auto& stage = jrc::Stage::get();
        const auto center = stage.get_drops().find_nearest_lootable_center(
            stage.get_player().get_position());
        return center ? center->x() : MISSING_COORDINATE;
    }

    int msdrop_nearest_y()
    {
        auto& stage = jrc::Stage::get();
        const auto center = stage.get_drops().find_nearest_lootable_center(
            stage.get_player().get_position());
        return center ? center->y() : MISSING_COORDINATE;
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

    int msinventory_equipped(int16_t slot)
    {
        return jrc::Stage::get().get_player().get_look().get_equips()
            .get_equip(static_cast<jrc::Equipslot::Id>(slot));
    }

    int msinventory_equipped_marker(int16_t slot)
    {
        const auto& inventory = jrc::Stage::get().get_player().get_inventory();
        const uint64_t cash_id = static_cast<uint64_t>(inventory.get_cash_id(
            jrc::InventoryType::EQUIPPED, slot));
        return static_cast<int32_t>(cash_id ^ (cash_id >> 32));
    }


    int msiteminventory_active()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items && items->is_active() ? 1 : 0;
    }

    int msiteminventory_x()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->layout_position().x() : -1;
    }

    int msiteminventory_y()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->layout_position().y() : -1;
    }

    int msiteminventory_tab()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? static_cast<int>(items->layout_tab()) : -1;
    }

    int msiteminventory_first_slot()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->visible_first_slot() : -1;
    }

    int msiteminventory_last_slot()
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->visible_last_slot() : -1;
    }

    int msiteminventory_slot_at(int x, int y)
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items ? items->hit_test(jrc::Point<int16_t>(
            static_cast<int16_t>(x), static_cast<int16_t>(y))) : -1;
    }

    int msiteminventory_tab_x(int32_t item_id)
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items && items->is_active()
            ? items->tab_button_center(
                jrc::InventoryType::by_item_id(item_id)).x()
            : MISSING_COORDINATE;
    }

    int msiteminventory_tab_y(int32_t item_id)
    {
        auto items = jrc::UI::get().get_element<jrc::UIItemInventory>();
        return items && items->is_active()
            ? items->tab_button_center(
                jrc::InventoryType::by_item_id(item_id)).y()
            : MISSING_COORDINATE;
    }

    int msnpc_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::NPCTALK) ? 1 : 0;
    }

    // NPC 对话框按钮/布局只读探针：返回 -9999 表示元素未激活。
    int msnpctalk_button_field(int which, int field)
    {
        auto talk = jrc::UI::get().get_element<jrc::UINpcTalk>();
        return talk && talk->is_active() ? talk->probe_button(which, field) : -9999;
    }

    int msnpctalk_element_field(int field)
    {
        auto talk = jrc::UI::get().get_element<jrc::UINpcTalk>();
        return talk && talk->is_active() ? talk->probe_element(field) : -9999;
    }

    int msui_questlog_rows()
    {
        auto window = jrc::UI::get().get_element<jrc::UIQuestLog>();
        return window ? window->row_count() : -1;
    }

    int msui_questlog_selected()
    {
        auto window = jrc::UI::get().get_element<jrc::UIQuestLog>();
        return window ? window->selected_id() : -1;
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

    const char* msstatusbar_stat_label(int which)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? statusbar->get_stat_label(static_cast<int16_t>(which))
            : "";
    }

    int msstatusbar_hit(int x, int y)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        if (!statusbar)
        {
            return -1;
        }
        const jrc::Point<int16_t> point(
            static_cast<int16_t>(x), static_cast<int16_t>(y));
        return (statusbar->is_active() ? 1 : 0)
            | (statusbar->is_in_range(point) ? 2 : 0);
    }

    int msstatusbar_button_at(int x, int y)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar
            ? static_cast<int>(statusbar->button_at(jrc::Point<int16_t>(
                static_cast<int16_t>(x), static_cast<int16_t>(y))))
            : -1;
    }

    int msstatusbar_press_count()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_press_count()) : -1;
    }

    int msstatusbar_last_button()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_last_button()) : -1;
    }

    int msstatusbar_cursor_x()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_cursor_x()) : -1;
    }

    int msstatusbar_cursor_y()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_cursor_y()) : -1;
    }

    int msstatusbar_rightclick_index()
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return statusbar ? static_cast<int>(statusbar->debug_rightclick_index()) : -2;
    }

    int msstatusbar_control_bounds(int id, int edge)
    {
        auto statusbar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        // Reject ids outside the uint16_t button range so the cast below
        // cannot wrap onto a real button (e.g. 65540 -> BT_MENU).
        if (!statusbar || id < 0 || id > 0xFFFF || edge < 0 || edge > 3)
        {
            return MISSING_COORDINATE;
        }
        const jrc::Rectangle<int16_t> bounds =
            statusbar->get_control_bounds(static_cast<uint16_t>(id));
        if (bounds.width() <= 0 || bounds.height() <= 0)
        {
            return MISSING_COORDINATE;
        }
        switch (edge)
        {
        case 0:
            return static_cast<int>(bounds.l());
        case 1:
            return static_cast<int>(bounds.t());
        case 2:
            return static_cast<int>(bounds.r());
        default:
            return static_cast<int>(bounds.b());
        }
    }

    int msui_front_at(int x, int y)
    {
        return static_cast<int>(jrc::UI::get().front_type_at(jrc::Point<int16_t>(
            static_cast<int16_t>(x), static_cast<int16_t>(y))));
    }

    int msui_cursor_x()
    {
        return static_cast<int>(jrc::UI::get().cursor_position().x());
    }

    int msui_cursor_y()
    {
        return static_cast<int>(jrc::UI::get().cursor_position().y());
    }

    int msskillbook_exists()
    {
        return jrc::UI::get().has_element(jrc::UIElement::SKILLBOOK) ? 1 : 0;
    }

    int msskillbook_selected()
    {
        auto skillbook = jrc::UI::get().get_element<jrc::UISkillbook>();
        return skillbook && skillbook->is_active()
            ? skillbook->get_selected_skill()
            : 0;
    }

    int msskillbook_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::SKILLBOOK) ? 1 : 0;
    }
}
