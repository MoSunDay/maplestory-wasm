#include "UIPetOptions.h"

#include "../../Components/AreaButton.h"
#include "../../UI.h"
#include "../UIStatusBar.h"

#include "../../../Gameplay/Pet/PetAutomation.h"
#include "../../../Gameplay/Stage.h"
#include "../../../Data/ItemData.h"
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
        : UIElement({}, { 283, 492 }),
          hp_threshold(Stage::get().get_pet_automation().get_hp_threshold()),
          mp_threshold(Stage::get().get_pet_automation().get_mp_threshold()),
          hp_item(Stage::get().get_pet_automation().get_hp_item()),
          mp_item(Stage::get().get_pet_automation().get_mp_item()),
          panel_background(283, 492, Geometry::BLACK, 0.92f),
          panel_header(283, 54, Geometry::WHITE, 0.12f),
          section_divider(235, 1, Geometry::WHITE, 0.28f),
          slider_track(SLIDER_WIDTH, 8, Geometry::WHITE, 0.30f),
          hp_fill(1, 8, Geometry::HPBAR_LIGHTRED, 1.0f),
          mp_fill(1, 8, Geometry::HPBAR_LIGHTGREEN, 1.0f),
          potion_background(POTION_WIDTH, POTION_HEIGHT, Geometry::WHITE, 0.10f),
          button_background(85, 28, Geometry::WHITE, 0.16f),
          title(Text::A13B, Text::CENTER, Text::WHITE, "PET AUTO RECOVERY"),
          help(Text::A11M, Text::CENTER, Text::LIGHTGREY,
               "Choose thresholds and items from USE inventory"),
          hp_label(Text::A12M, Text::LEFT, Text::WHITE, ""),
          mp_label(Text::A12M, Text::LEFT, Text::WHITE, ""),
          hp_potion_title(Text::A11M, Text::LEFT, Text::LIGHTGREY, "HP RECOVERY ITEM"),
          mp_potion_title(Text::A11M, Text::LEFT, Text::LIGHTGREY, "MP RECOVERY ITEM"),
          hp_potion_label(Text::A11M, Text::LEFT, Text::WHITE, "", 125),
          mp_potion_label(Text::A11M, Text::LEFT, Text::WHITE, "", 125),
          previous_arrow(Text::A13B, Text::CENTER, Text::YELLOW, "<"),
          next_arrow(Text::A13B, Text::CENTER, Text::YELLOW, ">"),
          potion_help(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "Click left / right to select; OFF disables it"),
          ok_label(Text::A12B, Text::CENTER, Text::WHITE, "OK"),
          cancel_label(Text::A12B, Text::CENTER, Text::WHITE, "CANCEL")
    {
        const Inventory& inventory = Stage::get().get_player().get_inventory();
        hp_candidates = potion_candidates(inventory, true);
        mp_candidates = potion_candidates(inventory, false);
        hp_item = available_selection(inventory, hp_item, true);
        mp_item = available_selection(inventory, mp_item, false);

        buttons[BT_OK] = std::make_unique<AreaButton>(
            Point<int16_t>(45, 448), Point<int16_t>(85, 28));
        buttons[BT_CANCEL] = std::make_unique<AreaButton>(
            Point<int16_t>(153, 448), Point<int16_t>(85, 28));
        sync_labels();
        sync_potion_labels();
    }

    void UIPetOptions::draw(float alpha) const
    {
        panel_background.draw(position);
        panel_header.draw(position);
        section_divider.draw(position + Point<int16_t>(24, 96));
        section_divider.draw(position + Point<int16_t>(24, 240));
        section_divider.draw(position + Point<int16_t>(24, 384));

        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));
        hp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        mp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));
        potion_background.draw(position + Point<int16_t>(POTION_LEFT, HP_POTION_Y));
        potion_background.draw(position + Point<int16_t>(POTION_LEFT, MP_POTION_Y));

        title.draw(position + Point<int16_t>(141, 30));
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
        potion_help.draw(position + Point<int16_t>(141, 413));
        button_background.draw(position + Point<int16_t>(45, 448));
        button_background.draw(position + Point<int16_t>(153, 448));
        ok_label.draw(position + Point<int16_t>(87, 456));
        cancel_label.draw(position + Point<int16_t>(195, 456));

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

    int32_t UIPetOptions::selected_hp_item() const
    {
        return hp_item;
    }

    int32_t UIPetOptions::selected_mp_item() const
    {
        return mp_item;
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
            if (Stage::get().set_pet_auto_thresholds(hp_threshold, mp_threshold))
            {
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
            }
            else if (Optional<UIStatusbar> statusbar =
                UI::get().get_element<UIStatusbar>())
            {
                statusbar->send_chatline(
                    "宠物自动补给阈值保存失败，请检查浏览器本地存储权限。",
                    UIChatbar::RED
                );
            }
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
        hp_label.change_text("Recover HP at or below " + std::to_string(hp_threshold) + "%");
        mp_label.change_text("Recover MP at or below " + std::to_string(mp_threshold) + "%");
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
}
