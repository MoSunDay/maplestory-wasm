#include "UIPetOptions.h"

#include "../../Components/AreaButton.h"
#include "../../UI.h"
#include "../UIStatusBar.h"

#include "../../../Gameplay/Pet/PetAutomation.h"
#include "../../../Gameplay/Pet/PetBuffPolicy.h"
#include "../../../Gameplay/Pet/PetVitalityPolicy.h"
#include "../../../Gameplay/Stage.h"
#include "../../../Data/ItemData.h"
#include "../../../Data/SkillData.h"
#include "../../../Net/Packets/PlayerPackets.h"

#include <algorithm>
#include <string>
#include <vector>

namespace jrc
{
    namespace
    {
        constexpr int16_t SLIDER_LEFT = 52;
        constexpr int16_t SLIDER_WIDTH = 180;
        constexpr int16_t HP_SLIDER_Y = 150;
        constexpr int16_t HP_POTION_Y = 202;
        constexpr int16_t MP_SLIDER_Y = 294;
        constexpr int16_t MP_POTION_Y = 346;
        constexpr int16_t POTION_LEFT = 42;
        constexpr int16_t POTION_WIDTH = 200;
        constexpr int16_t POTION_HEIGHT = 38;
        constexpr int16_t PANEL_WIDTH = 283;
        constexpr int16_t PANEL_HEIGHT = 580;
        constexpr int16_t BUTTON_Y = 536;
        constexpr int16_t BUFF_DIVIDER_Y = 420;
        constexpr int16_t BUFF_TITLE_Y = 428;
        constexpr int16_t BUFF_SLOT_SIZE = 32;
        constexpr int16_t BUFF_SLOT_GAP = 4;
        constexpr int16_t BUFF_COLUMNS = 6;
        constexpr int16_t BUFF_GRID_LEFT = 35;
        constexpr int16_t BUFF_GRID_TOP = 448;

        std::vector<int32_t> potion_candidates(const Inventory& inventory, bool hp)
        {
            std::vector<int32_t> result = { 0 };
            for (int16_t slot = 1; slot <= inventory.get_slotmax(InventoryType::USE); ++slot)
            {
                int32_t item_id = inventory.get_item_id(InventoryType::USE, slot);
                const ItemData& item = ItemData::get(item_id);
                bool eligible = item_id > 0 && item &&
                    (hp ? item.restores_hp() : item.restores_mp());
                if (eligible && std::find(result.begin(), result.end(), item_id) == result.end())
                    result.push_back(item_id);
            }
            std::sort(result.begin() + 1, result.end());
            return result;
        }

        int32_t available_selection(const Inventory& inventory, int32_t item_id, bool hp)
        {
            if (item_id == 0)
                return 0;
            const ItemData& item = ItemData::get(item_id);
            return inventory.find_item(InventoryType::USE, item_id) > 0 && item &&
                (hp ? item.restores_hp() : item.restores_mp()) ? item_id : 0;
        }

        int32_t total_item_count(const Inventory& inventory, int32_t item_id)
        {
            int32_t total = 0;
            for (int16_t slot = 1; slot <= inventory.get_slotmax(InventoryType::USE); ++slot)
                if (inventory.get_item_id(InventoryType::USE, slot) == item_id)
                    total += inventory.get_item_count(InventoryType::USE, slot);
            return total;
        }
    }

    UIPetOptions::UIPetOptions()
        : UIElement({}, { PANEL_WIDTH, PANEL_HEIGHT }),
          hp_threshold(Stage::get().get_pet_automation().get_hp_threshold()),
          mp_threshold(Stage::get().get_pet_automation().get_mp_threshold()),
          hp_item(Stage::get().get_pet_automation().get_hp_item()),
          mp_item(Stage::get().get_pet_automation().get_mp_item()),
          buff_skills(Stage::get().get_pet_automation().get_buff_skills()),
          panel_background(PANEL_WIDTH, PANEL_HEIGHT, Geometry::BLACK, 0.92f),
          panel_header(PANEL_WIDTH, 54, Geometry::WHITE, 0.12f),
          section_divider(235, 1, Geometry::WHITE, 0.28f),
          slider_track(SLIDER_WIDTH, 8, Geometry::WHITE, 0.30f),
          hp_fill(1, 8, Geometry::HPBAR_LIGHTRED, 1.0f),
          mp_fill(1, 8, Geometry::HPBAR_LIGHTGREEN, 1.0f),
          potion_background(POTION_WIDTH, POTION_HEIGHT, Geometry::WHITE, 0.10f),
          buff_slot_background(BUFF_SLOT_SIZE, BUFF_SLOT_SIZE, Geometry::WHITE, 0.10f),
          button_background(85, 28, Geometry::WHITE, 0.16f),
          pet_status(Text::A11M, Text::CENTER, Text::WHITE, ""),
          title(Text::A13B, Text::CENTER, Text::WHITE, "宠物自动恢复"),
          help(Text::A11M, Text::CENTER, Text::LIGHTGREY,
               "设置触发比例并从消耗栏选择道具"),
          hp_label(Text::A12M, Text::LEFT, Text::WHITE, ""),
          mp_label(Text::A12M, Text::LEFT, Text::WHITE, ""),
          hp_potion_title(Text::A11M, Text::LEFT, Text::LIGHTGREY, "HP 恢复道具"),
          mp_potion_title(Text::A11M, Text::LEFT, Text::LIGHTGREY, "MP 恢复道具"),
          hp_potion_label(Text::A11M, Text::LEFT, Text::WHITE, "", 125),
          mp_potion_label(Text::A11M, Text::LEFT, Text::WHITE, "", 125),
          previous_arrow(Text::A13B, Text::CENTER, Text::YELLOW, "<"),
          next_arrow(Text::A13B, Text::CENTER, Text::YELLOW, ">"),
          potion_help(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "点击左/右切换；OFF 为关闭"),
          buff_title(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "自动 BUFF（拖入技能，右键移除）"),
          ok_label(Text::A12B, Text::CENTER, Text::WHITE, "确定"),
          cancel_label(Text::A12B, Text::CENTER, Text::WHITE, "取消")
    {
        const Inventory& inventory = Stage::get().get_player().get_inventory();
        hp_candidates = potion_candidates(inventory, true);
        mp_candidates = potion_candidates(inventory, false);
        hp_item = available_selection(inventory, hp_item, true);
        mp_item = available_selection(inventory, mp_item, false);

        buttons[BT_OK] = std::make_unique<AreaButton>(
            Point<int16_t>(45, BUTTON_Y), Point<int16_t>(85, 28));
        buttons[BT_CANCEL] = std::make_unique<AreaButton>(
            Point<int16_t>(153, BUTTON_Y), Point<int16_t>(85, 28));
        sync_labels();
        sync_potion_labels();
    }

    void UIPetOptions::update()
    {
        UIElement::update();
        sync_pet_status();
    }

    void UIPetOptions::sync_pet_status()
    {
        const Player& player = Stage::get().get_player();
        const PetLook* pet = player.get_pet(0);
        const Inventory& inventory = player.get_inventory();
        const Pet* petinfo = pet ? inventory.get_pet_by_cash_id(pet->get_uniqueid()) : nullptr;

        pet_vitality::State state = pet_vitality::classify(
            petinfo != nullptr, petinfo ? petinfo->get_fullness() : 0);

        std::string text = pet_vitality::describe(state);
        Text::Color color = Text::LIGHTGREY;
        if (petinfo)
        {
            text = petinfo->get_name() + "  Lv." +
                std::to_string(petinfo->get_level()) + "  Fullness " +
                std::to_string(petinfo->get_fullness());
            color = Text::WHITE;
        }

        if (state == pet_vitality::State::HUNGRY)
        {
            text += " - HUNGRY!";
            color = Text::ORANGE;
        }

        if (text != pet_status_cache)
        {
            pet_status_cache = text;
            pet_status.change_color(color);
            pet_status.change_text(text);
        }
    }

    void UIPetOptions::draw(float alpha) const
    {
        panel_background.draw(position);
        panel_header.draw(position);
        section_divider.draw(position + Point<int16_t>(24, 96));
        section_divider.draw(position + Point<int16_t>(24, 240));
        section_divider.draw(position + Point<int16_t>(24, 384));
        section_divider.draw(position + Point<int16_t>(24, BUFF_DIVIDER_Y));

        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));
        hp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        mp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));
        potion_background.draw(position + Point<int16_t>(POTION_LEFT, HP_POTION_Y));
        potion_background.draw(position + Point<int16_t>(POTION_LEFT, MP_POTION_Y));

        title.draw(position + Point<int16_t>(141, 30));
        pet_status.draw(position + Point<int16_t>(141, 56));
        help.draw(position + Point<int16_t>(141, 78));
        hp_label.draw(position + Point<int16_t>(52, 119));
        mp_label.draw(position + Point<int16_t>(52, 263));
        hp_potion_title.draw(position + Point<int16_t>(52, 181));
        mp_potion_title.draw(position + Point<int16_t>(52, 325));
        hp_potion_label.draw(position + Point<int16_t>(101, 214));
        mp_potion_label.draw(position + Point<int16_t>(101, 358));
        previous_arrow.draw(position + Point<int16_t>(56, 214));
        previous_arrow.draw(position + Point<int16_t>(56, 358));
        next_arrow.draw(position + Point<int16_t>(228, 214));
        next_arrow.draw(position + Point<int16_t>(228, 358));
        potion_help.draw(position + Point<int16_t>(141, 405));
        buff_title.draw(position + Point<int16_t>(141, BUFF_TITLE_Y));
        for (size_t slot = 0; slot < buff_skills.size(); ++slot)
            buff_slot_background.draw(position + buff_slot_offset(slot));
        for (size_t slot = 0; slot < buff_skills.size(); ++slot)
        {
            const int32_t skill_id = buff_skills[slot];
            if (skill_id <= 0)
                continue;
            const SkillData& skill = SkillData::get(skill_id);
            const Texture& icon = skill.get_icon(SkillData::NORMAL);
            if (skill.get_masterlevel() > 0 && icon.is_valid())
                icon.draw(position + buff_slot_offset(slot) + Point<int16_t>(0, 32));
        }
        button_background.draw(position + Point<int16_t>(45, BUTTON_Y));
        button_background.draw(position + Point<int16_t>(153, BUTTON_Y));
        ok_label.draw(position + Point<int16_t>(87, BUTTON_Y + 8));
        cancel_label.draw(position + Point<int16_t>(195, BUTTON_Y + 8));

        if (hp_item > 0)
            ItemData::get(hp_item).get_icon(false).draw(position + Point<int16_t>(76, 205));
        if (mp_item > 0)
            ItemData::get(mp_item).get_icon(false).draw(position + Point<int16_t>(76, 349));
        draw_buttons(alpha);
    }

    void UIPetOptions::update_screen(int16_t width, int16_t height)
    {
        position = {
            static_cast<int16_t>((width - dimension.x()) / 2),
            static_cast<int16_t>(std::max<int16_t>(0, (height - dimension.y()) / 2))
        };
    }

    UIElement::CursorResult UIPetOptions::send_cursor(
        bool pressed, Point<int16_t> cursorpos)
    {
        if (CursorResult button = UIElement::send_cursor(pressed, cursorpos))
            return button;

        constexpr bool TYPES[] = { true, false };
        for (bool hp : TYPES)
        {
            Rectangle<int16_t> bounds = slider_bounds(hp);
            if (bounds.contains(cursorpos))
            {
                if (pressed)
                {
                    int32_t value = 1 +
                        (cursorpos.x() - bounds.l()) * 98 / std::max(1, bounds.width() - 1);
                    set_threshold(hp, value);
                    return { Cursor::CLICKING, true };
                }
                return { Cursor::CANCLICK, true };
            }

            Rectangle<int16_t> potion = potion_bounds(hp);
            if (potion.contains(cursorpos))
            {
                if (pressed)
                {
                    int32_t direction = cursorpos.x() < potion.l() + potion.width() / 2 ? -1 : 1;
                    select_potion(hp, direction);
                    return { Cursor::CLICKING, true };
                }
                return { Cursor::CANCLICK, true };
            }
        }
        return { Cursor::IDLE, false };
    }

    void UIPetOptions::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            deactivate();
    }

    void UIPetOptions::send_icon(const Icon& icon, Point<int16_t> cursorpos)
    {
        const int32_t skill_id = icon.get_skill_id();
        if (skill_id <= 0)
            return;

        const int slot = buff_slot_at(cursorpos);
        if (slot < 0)
            return;

        const int32_t level = Stage::get().get_player().get_skilllevel(skill_id);
        if (!pet_buff::skill_can_auto_buff(SkillData::get(skill_id),
            skill_id, level))
            return;

        // Draft-only edit: one slot per skill, persisted later by OK.
        for (int32_t& id : buff_skills)
        {
            if (id == skill_id)
                id = 0;
        }
        buff_skills[static_cast<size_t>(slot)] = skill_id;
    }

    bool UIPetOptions::accepts_drop(DragKind kind, Point<int16_t> cursorpos) const
    {
        // Only skill icons can be assigned to an auto-buff slot, and only
        // while the cursor is over a slot. Skill eligibility is re-checked
        // in send_icon, which owns the draft edit.
        return kind == DragKind::SKILL && buff_slot_at(cursorpos) >= 0;
    }

    void UIPetOptions::rightclick(Point<int16_t> cursorpos)
    {
        const int slot = buff_slot_at(cursorpos);
        if (slot < 0)
            return;

        buff_skills[static_cast<size_t>(slot)] = 0;
    }

    Point<int16_t> UIPetOptions::buff_slot_center(size_t slot) const
    {
        if (slot >= pet_automation::MAX_BUFF_SKILLS)
            return { -1, -1 };
        return position + buff_slot_offset(slot) +
            Point<int16_t>(BUFF_SLOT_SIZE / 2, BUFF_SLOT_SIZE / 2);
    }

    Point<int16_t> UIPetOptions::slider_center(bool hp) const
    {
        Rectangle<int16_t> bounds = slider_bounds(hp);
        return {
            static_cast<int16_t>(bounds.l() + bounds.width() / 2),
            static_cast<int16_t>(bounds.t() + bounds.height() / 2)
        };
    }

    int16_t UIPetOptions::slider_x_for_percent(bool hp, int percent) const
    {
        Rectangle<int16_t> bounds = slider_bounds(hp);
        const int span = std::max(1, bounds.width() - 1);
        const int value = std::clamp(percent, 1, 99);
        // Integer ceil of (value - 1) * span / 98, inverting the send_cursor
        // mapping 1 + (x - l) * 98 / span. The forward reading of the
        // returned x is exact for every value, so 98 resolves to r - 2.
        const int offset = ((value - 1) * span + 97) / 98;
        return static_cast<int16_t>(bounds.l() + offset);
    }

    Point<int16_t> UIPetOptions::potion_center(bool hp, bool) const
    {
        // One selector row per resource: the arrows cycle the candidates in
        // place, so the "next" slot is the same slot.
        Rectangle<int16_t> bounds = potion_bounds(hp);
        return {
            static_cast<int16_t>(bounds.l() + bounds.width() / 2),
            static_cast<int16_t>(bounds.t() + bounds.height() / 2)
        };
    }

    Point<int16_t> UIPetOptions::ok_center() const
    {
        auto button = buttons.find(BT_OK);
        if (button == buttons.end())
            return { -1, -1 };
        Rectangle<int16_t> bounds = button->second->bounds(position);
        return {
            static_cast<int16_t>(bounds.l() + bounds.width() / 2),
            static_cast<int16_t>(bounds.t() + bounds.height() / 2)
        };
    }

    int32_t UIPetOptions::selected_hp_item() const
    {
        return hp_item;
    }

    int32_t UIPetOptions::selected_mp_item() const
    {
        return mp_item;
    }

    int32_t UIPetOptions::draft_buff_skill(size_t slot) const
    {
        if (slot >= pet_automation::MAX_BUFF_SKILLS)
            return 0;
        return buff_skills[slot] > 0 ? buff_skills[slot] : 0;
    }

    Button::State UIPetOptions::button_pressed(uint16_t buttonid)
    {
        if (buttonid == BT_CANCEL)
        {
            deactivate();
            return Button::NORMAL;
        }
        if (buttonid == BT_OK)
        {
            const pet_automation::BuffSkillSlots previous =
                Stage::get().get_pet_automation().get_buff_skills();
            auto show_save_failure = []() {
                if (Optional<UIStatusbar> statusbar =
                    UI::get().get_element<UIStatusbar>())
                {
                    statusbar->send_chatline(
                        "宠物自动补给阈值保存失败，请检查浏览器本地存储权限。",
                        UIChatbar::RED
                    );
                }
            };

            if (!Stage::get().apply_pet_buff_skills(buff_skills))
            {
                show_save_failure();
                return Button::NORMAL;
            }
            if (!Stage::get().set_pet_auto_thresholds(hp_threshold, mp_threshold))
            {
                Stage::get().apply_pet_buff_skills(previous);
                show_save_failure();
                return Button::NORMAL;
            }

            const Inventory& inventory = Stage::get().get_player().get_inventory();
            hp_item = available_selection(inventory, hp_item, true);
            mp_item = available_selection(inventory, mp_item, false);
            const auto& automation = Stage::get().get_pet_automation();
            if (automation.get_hp_item() != hp_item)
            {
                ChangeKeyMapPacket(1, hp_item).dispatch();
                Stage::get().set_pet_auto_hp_item(hp_item);
            }
            if (automation.get_mp_item() != mp_item)
            {
                ChangeKeyMapPacket(2, mp_item).dispatch();
                Stage::get().set_pet_auto_mp_item(mp_item);
            }
            deactivate();
            return Button::NORMAL;
        }
        return Button::PRESSED;
    }

    void UIPetOptions::set_threshold(bool hp, int32_t value)
    {
        uint8_t threshold = pet_automation::clamp_threshold(value);
        if (hp)
            hp_threshold = threshold;
        else
            mp_threshold = threshold;
        sync_labels();
    }

    void UIPetOptions::select_potion(bool hp, int32_t direction)
    {
        if (hp)
            hp_item = pet_automation::select_next_item(hp_candidates, hp_item, direction);
        else
            mp_item = pet_automation::select_next_item(mp_candidates, mp_item, direction);
        sync_potion_labels();
    }

    void UIPetOptions::sync_labels()
    {
        hp_label.change_text("HP 不高于 " + std::to_string(hp_threshold) + "% 时自动恢复");
        mp_label.change_text("MP 不高于 " + std::to_string(mp_threshold) + "% 时自动恢复");
        hp_fill.setwidth(static_cast<int16_t>(SLIDER_WIDTH * hp_threshold / 100));
        mp_fill.setwidth(static_cast<int16_t>(SLIDER_WIDTH * mp_threshold / 100));
    }

    void UIPetOptions::sync_potion_labels()
    {
        const Inventory& inventory = Stage::get().get_player().get_inventory();
        auto label = [&inventory](int32_t item_id) {
            if (item_id == 0)
                return std::string("OFF");
            const ItemData& item = ItemData::get(item_id);
            return item.get_name() + " x" + std::to_string(total_item_count(inventory, item_id));
        };
        hp_potion_label.change_text(label(hp_item));
        mp_potion_label.change_text(label(mp_item));
    }

    Rectangle<int16_t> UIPetOptions::slider_bounds(bool hp) const
    {
        int16_t y = hp ? HP_SLIDER_Y : MP_SLIDER_Y;
        return {
            position + Point<int16_t>(SLIDER_LEFT, static_cast<int16_t>(y - 8)),
            position + Point<int16_t>(SLIDER_LEFT + SLIDER_WIDTH, static_cast<int16_t>(y + 16))
        };
    }

    Rectangle<int16_t> UIPetOptions::potion_bounds(bool hp) const
    {
        int16_t y = hp ? HP_POTION_Y : MP_POTION_Y;
        return {
            position + Point<int16_t>(POTION_LEFT, y),
            position + Point<int16_t>(POTION_LEFT + POTION_WIDTH,
                static_cast<int16_t>(y + POTION_HEIGHT))
        };
    }

    Point<int16_t> UIPetOptions::buff_slot_offset(size_t slot) const
    {
        if (slot >= pet_automation::MAX_BUFF_SKILLS)
            return {};

        const int16_t column = static_cast<int16_t>(slot % BUFF_COLUMNS);
        const int16_t row = static_cast<int16_t>(slot / BUFF_COLUMNS);
        return {
            static_cast<int16_t>(BUFF_GRID_LEFT + column * (BUFF_SLOT_SIZE + BUFF_SLOT_GAP)),
            static_cast<int16_t>(BUFF_GRID_TOP + row * (BUFF_SLOT_SIZE + BUFF_SLOT_GAP))
        };
    }

    Rectangle<int16_t> UIPetOptions::buff_slot_bounds(size_t slot) const
    {
        Point<int16_t> offset = buff_slot_offset(slot);
        return {
            position + offset,
            position + offset + Point<int16_t>(BUFF_SLOT_SIZE, BUFF_SLOT_SIZE)
        };
    }

    int UIPetOptions::buff_slot_at(Point<int16_t> cursorpos) const
    {
        for (size_t slot = 0; slot < pet_automation::MAX_BUFF_SKILLS; ++slot)
        {
            if (buff_slot_bounds(slot).contains(cursorpos))
                return static_cast<int>(slot);
        }
        return -1;
    }
}
