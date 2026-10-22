#include "UIUserInfo.h"

#include "../../Character/Job.h"
#include "../../Constants.h"
#include "../../Data/EquipData.h"
#include "../../Data/ItemData.h"
#include "../../Net/CharacterInfo/CharacterInfoPackets.h"
#include "../../Net/CharacterInfo/CharacterInfoProtocol.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/TradePackets.h"
#include "../../Gameplay/Stage.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../UITypes/UIChatBar.h"
#include "../UITypes/UIStatusBar.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <utility>

namespace jrc
{
    namespace
    {
        constexpr int16_t MAIN_WIDTH = 271;
        constexpr int16_t MAIN_HEIGHT = 190;
        constexpr int16_t PET_TOP = 191;
        constexpr int16_t PET_HEIGHT = 196;
        constexpr int16_t EXPANDED_HEIGHT = PET_TOP + PET_HEIGHT;
        constexpr uint16_t RESPONSE_TIMEOUT_FRAMES = 625;

        Text field_text(const std::string& value, uint16_t width = 150)
        {
            return Text(Text::A11M, Text::LEFT, Text::DARKGREY, value, width);
        }

        void show_chat_message(const std::string& message,
            UIChatbar::LineType type)
        {
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
                statusbar->send_chatline(message, type);
        }
    }

    UIUserInfo::UIUserInfo(int32_t id, const std::string& name,
        const CharLook& target_look, bool is_self)
        : character_id(id), character_name(name), look(target_look), self(is_self),
          title(Text::A11M, Text::CENTER, Text::WHITE, name)
    {
        nl::node source = nl::nx::ui["UIWindow2.img"]["UserInfo"];
        nl::node character = source["character"];
        nl::node pet = source["pet"];
        const char* backgrounds[] = { "backgrnd", "backgrnd2", "backgrnd3" };
        for (size_t index = 0; index < character_backgrounds.size(); ++index)
        {
            character_backgrounds[index] = character[backgrounds[index]];
            pet_backgrounds[index] = pet[backgrounds[index]];
            pet_action_backgrounds[index] =
                pet["ActionPet"][std::to_string(index)]["0"];
            enabled_tabs[index] = pet["Tab"]["enabled"][std::to_string(index)];
            disabled_tabs[index] = pet["Tab"]["disabled"][std::to_string(index)];
        }
        character_name_background = character["name"];
        married_icon = character["married"];

        nl::node basic = nl::nx::ui["Basic.img"];
        buttons[BT_CLOSE] = std::make_unique<MapleButton>(
            basic["BtClose"], Point<int16_t>(248, 7));
        buttons[BT_PET] = std::make_unique<MapleButton>(character["BtPet"]);
        buttons[BT_TRADE] = std::make_unique<MapleButton>(character["BtTrad"]);
        buttons[BT_PARTY] = std::make_unique<MapleButton>(character["BtParty"]);
        buttons[BT_FAME_UP] = std::make_unique<MapleButton>(character["BtPopUp"]);
        buttons[BT_FAME_DOWN] = std::make_unique<MapleButton>(character["BtPopDown"]);
        for (uint16_t index = 0; index < 3; ++index)
        {
            buttons[BT_PET_TAB_0 + index] = std::make_unique<AreaButton>(
                Point<int16_t>(10 + index * 84, PET_TOP + 11),
                Point<int16_t>(83, 13));
            buttons[BT_PET_TAB_0 + index]->set_active(false);
        }

        position = { 110, 90 };
        dimension = { MAIN_WIDTH, MAIN_HEIGHT };
        active = true;
        can_fame = !self && Stage::get().get_player().get_level() >= 15;
        sync_controls();
        CharacterInfoRequestPacket(character_id).dispatch();
    }

    void UIUserInfo::draw(float alpha) const
    {
        draw_main(alpha);
        if (expanded)
            draw_pet(alpha);
        draw_buttons(alpha);
    }

    void UIUserInfo::draw_main(float alpha) const
    {
        for (const Texture& background : character_backgrounds)
            background.draw({ position });
        character_name_background.draw({ position });
        look.draw(position + Point<int16_t>(63, 129), false,
            Stance::STAND1, Expression::DEFAULT);
        title.draw(position + Point<int16_t>(59, 131));

        if (info)
        {
            const Job job(info->job_id);
            field_text(std::to_string(info->level))
                .draw(position + Point<int16_t>(153, 65));
            field_text(job.get_name())
                .draw(position + Point<int16_t>(153, 83));
            field_text(std::to_string(info->fame))
                .draw(position + Point<int16_t>(153, 101));
            field_text(info->guild.empty() ? "无" : info->guild)
                .draw(position + Point<int16_t>(153, 119));
            field_text(info->alliance.empty() ? "无" : info->alliance)
                .draw(position + Point<int16_t>(153, 137));
            if (info->married)
                married_icon.draw({ position });
        }
        (void)alpha;
    }

    void UIUserInfo::draw_pet(float alpha) const
    {
        const Point<int16_t> pet_origin = position + Point<int16_t>(0, PET_TOP);
        for (const Texture& background : pet_backgrounds)
            background.draw({ pet_origin });
        if (info && !info->pets.empty())
            pet_action_backgrounds[pet_index].draw({ pet_origin });
        for (size_t index = 0; index < enabled_tabs.size(); ++index)
        {
            const bool selected = info && index < info->pets.size() && index == pet_index;
            (selected ? enabled_tabs[index] : disabled_tabs[index]).draw({ pet_origin });
        }
        if (!info || info->pets.empty())
            return;

        const character_info::PetInfo& pet = info->pets[pet_index];
        const ItemData& item = ItemData::get(pet.item_id);
        pet_animation.draw(DrawArgument(
            pet_origin + Point<int16_t>(53, 126)), alpha);
        Text(Text::A11M, Text::CENTER, Text::WHITE,
            pet.name.empty() ? item.get_name() : pet.name)
            .draw(pet_origin + Point<int16_t>(53, 128));
        field_text(item ? item.get_name() : std::to_string(pet.item_id), 100)
            .draw(pet_origin + Point<int16_t>(43, 153));
        field_text(std::to_string(pet.level), 50)
            .draw(pet_origin + Point<int16_t>(166, 153));
        field_text(std::to_string(pet.closeness), 100)
            .draw(pet_origin + Point<int16_t>(43, 171));
        field_text(std::to_string(pet.fullness), 50)
            .draw(pet_origin + Point<int16_t>(166, 171));
        if (pet.equipment_id > 0)
        {
            const EquipData& equip = EquipData::get(pet.equipment_id);
            if (equip)
            {
                const ItemData& equipment_item = equip.get_itemdata();
                equipment_item.get_icon(false).draw(
                    pet_origin + Point<int16_t>(104, 54));
                field_text(equipment_item.get_name(), 115)
                    .draw(pet_origin + Point<int16_t>(143, 31));
                field_text(std::to_string(
                    equip.get_reqstat(Maplestat::LEVEL)), 45)
                    .draw(pet_origin + Point<int16_t>(179, 49));
            }
        }
    }

    void UIUserInfo::update()
    {
        UIElement::update();
        look.update(Constants::TIMESTEP);
        if (expanded)
            pet_animation.update(Constants::TIMESTEP);
        if (loading && ++request_frames >= RESPONSE_TIMEOUT_FRAMES)
            fail_info("人物信息读取超时，请重新打开。");
        if (waiting_for_fame && ++fame_frames >= RESPONSE_TIMEOUT_FRAMES)
            fail_fame("人气操作超时，请稍后重试。");
    }

    void UIUserInfo::update_screen(int16_t, int16_t)
    {
        clamp_position();
    }

    bool UIUserInfo::remove_cursor(bool clicked, Point<int16_t> cursor)
    {
        if (dragged)
        {
            if (clicked)
                position = cursor - cursor_offset;
            else
                dragged = false;
            clamp_position();
            return true;
        }
        return UIElement::remove_cursor(clicked, cursor);
    }

    UIElement::CursorResult UIUserInfo::send_cursor(bool clicked,
        Point<int16_t> cursor)
    {
        if (dragged)
        {
            remove_cursor(clicked, cursor);
            return { clicked ? Cursor::CLICKING : Cursor::IDLE, true };
        }
        if (CursorResult result = UIElement::send_cursor(clicked, cursor))
            return result;
        if (clicked && cursor.y() >= position.y() && cursor.y() <= position.y() + 24)
        {
            cursor_offset = cursor - position;
            dragged = true;
            return { Cursor::CLICKING, true };
        }
        return { Cursor::IDLE, false };
    }

    void UIUserInfo::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            deactivate();
    }

    void UIUserInfo::apply_info(character_info::CharacterInfoPayload payload)
    {
        if (payload.character_id != character_id)
            return;
        info = std::move(payload);
        loading = false;
        request_frames = 0;
        pet_index = 0;
        refresh_pet_animation();
        if (!self && !can_fame)
            show_chat_message("15级后才能评价其他玩家的人气。", UIChatbar::YELLOW);
        sync_controls();
    }

    void UIUserInfo::fail_info(const std::string& message)
    {
        loading = false;
        request_frames = 0;
        show_chat_message(message, UIChatbar::RED);
        sync_controls();
    }

    void UIUserInfo::apply_fame_response(const character_info::FameResponse& response)
    {
        if (!waiting_for_fame)
            return;
        if (response.status == character_info::FameStatus::SUCCESS &&
            response.character_name != character_name)
        {
            return;
        }
        waiting_for_fame = false;
        fame_frames = 0;
        if (response.status == character_info::FameStatus::SUCCESS && info &&
            response.character_name == character_name)
        {
            info->fame = response.new_fame;
        }
        show_chat_message(character_info::fame_status_message(response.status),
            response.status == character_info::FameStatus::SUCCESS ?
                UIChatbar::YELLOW : UIChatbar::RED);
        sync_controls();
    }

    void UIUserInfo::fail_fame(const std::string& message)
    {
        if (!waiting_for_fame)
            return;
        waiting_for_fame = false;
        fame_frames = 0;
        show_chat_message(message, UIChatbar::RED);
        sync_controls();
    }

    Button::State UIUserInfo::button_pressed(uint16_t button_id)
    {
        switch (button_id)
        {
        case BT_CLOSE:
            deactivate();
            break;
        case BT_PET:
            expanded = !expanded;
            dimension = { MAIN_WIDTH,
                expanded ? EXPANDED_HEIGHT : MAIN_HEIGHT };
            clamp_position();
            sync_controls();
            break;
        case BT_TRADE:
            CreateTradePacket().dispatch();
            InviteTradePacket(character_id).dispatch();
            break;
        case BT_PARTY:
            InviteToPartyPacket(character_name).dispatch();
            break;
        case BT_FAME_UP:
            request_fame(true);
            break;
        case BT_FAME_DOWN:
            request_fame(false);
            break;
        default:
            if (button_id >= BT_PET_TAB_0 && button_id <= BT_PET_TAB_2 && info)
            {
                const uint8_t requested = static_cast<uint8_t>(button_id - BT_PET_TAB_0);
                if (requested < info->pets.size())
                {
                    pet_index = requested;
                    refresh_pet_animation();
                }
            }
            break;
        }
        const auto button = buttons.find(button_id);
        return button == buttons.end() ? Button::NORMAL : button->second->get_state();
    }

    void UIUserInfo::request_fame(bool increase)
    {
        GiveFamePacket(character_id, increase).dispatch();
        waiting_for_fame = true;
        fame_frames = 0;
        show_chat_message(increase ? "正在增加人气……" : "正在降低人气……",
            UIChatbar::YELLOW);
        sync_controls();
    }

    void UIUserInfo::refresh_pet_animation()
    {
        pet_animation = Animation();
        if (!info || pet_index >= info->pets.size())
            return;

        const std::string item_id = std::to_string(info->pets[pet_index].item_id);
        pet_animation = Animation(
            nl::nx::item["Pet"][item_id + ".img"]["stand0"]);
    }

    void UIUserInfo::sync_controls()
    {
        const bool ready = info.has_value();
        const bool has_pets = ready && !info->pets.empty();
        if (!has_pets)
            expanded = false;
        dimension = { MAIN_WIDTH, expanded ? EXPANDED_HEIGHT : MAIN_HEIGHT };
        buttons[BT_PET]->set_state(has_pets ? Button::NORMAL : Button::DISABLED);
        const Button::State social_state = ready && can_fame && !waiting_for_fame ?
            Button::NORMAL : Button::DISABLED;
        buttons[BT_TRADE]->set_state(ready && !self ? Button::NORMAL : Button::DISABLED);
        buttons[BT_PARTY]->set_state(ready && !self ? Button::NORMAL : Button::DISABLED);
        buttons[BT_FAME_UP]->set_state(social_state);
        buttons[BT_FAME_DOWN]->set_state(social_state);
        for (uint16_t index = 0; index < 3; ++index)
        {
            buttons[BT_PET_TAB_0 + index]->set_active(
                expanded && ready && index < info->pets.size());
        }
        clamp_position();
    }

    void UIUserInfo::clamp_position()
    {
        const int16_t max_x = std::max<int16_t>(0, Constants::viewwidth() - dimension.x());
        const int16_t max_y = std::max<int16_t>(0, Constants::viewheight() - dimension.y());
        position = { std::clamp<int16_t>(position.x(), 0, max_x),
            std::clamp<int16_t>(position.y(), 0, max_y) };
    }

    int32_t UIUserInfo::target_id() const { return character_id; }
    bool UIUserInfo::info_loaded() const { return info.has_value(); }
    bool UIUserInfo::pet_expanded() const { return expanded; }
    size_t UIUserInfo::pet_count() const { return info ? info->pets.size() : 0; }
    uint8_t UIUserInfo::selected_pet() const { return pet_index; }
    bool UIUserInfo::fame_pending() const { return waiting_for_fame; }
    Point<int16_t> UIUserInfo::origin() const { return position; }
    Point<int16_t> UIUserInfo::size() const { return dimension; }
}
