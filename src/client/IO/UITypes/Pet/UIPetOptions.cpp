#include "UIPetOptions.h"

#include "../../Components/MapleButton.h"
#include "../../UI.h"
#include "../UIStatusBar.h"

#include "../../../Gameplay/Pet/PetAutomation.h"
#include "../../../Gameplay/Stage.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        constexpr int16_t SLIDER_LEFT = 52;
        constexpr int16_t SLIDER_WIDTH = 180;
        constexpr int16_t HP_SLIDER_Y = 176;
        constexpr int16_t MP_SLIDER_Y = 276;
    }

    UIPetOptions::UIPetOptions()
        : UIElement({}, { 283, 558 }),
          hp_threshold(Stage::get().get_pet_automation().get_hp_threshold()),
          mp_threshold(Stage::get().get_pet_automation().get_mp_threshold()),
          slider_track(SLIDER_WIDTH, 8, Geometry::BLACK, 0.65f),
          hp_fill(1, 8, Geometry::HPBAR_LIGHTRED, 1.0f),
          mp_fill(1, 8, Geometry::HPBAR_LIGHTGREEN, 1.0f),
          title(Text::A13B, Text::CENTER, Text::WHITE, "宠物自动补给"),
          help(Text::A11M, Text::CENTER, Text::LIGHTGREY,
               "点击滑条设置触发比例（1% - 99%）"),
          hp_label(Text::A12M, Text::LEFT, Text::WHITE, ""),
          mp_label(Text::A12M, Text::LEFT, Text::WHITE, "")
    {
        nl::node source = nl::nx::ui["UIWindow2.img"]["OptionMenu"];
        sprites.emplace_back(source["backgrnd"]);
        background2 = source["backgrnd2"];
        background3 = source["backgrnd3"];
        buttons[BT_OK] = std::make_unique<MapleButton>(source["BtOK"], 78, 522);
        buttons[BT_CANCEL] = std::make_unique<MapleButton>(source["BtCancle"], 165, 522);
        sync_labels();
    }

    void UIPetOptions::draw(float alpha) const
    {
        draw_sprites(alpha);
        background2.draw(position);
        background3.draw(position);

        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        slider_track.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));
        hp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, HP_SLIDER_Y));
        mp_fill.draw(position + Point<int16_t>(SLIDER_LEFT, MP_SLIDER_Y));

        title.draw(position + Point<int16_t>(141, 30));
        help.draw(position + Point<int16_t>(141, 92));
        hp_label.draw(position + Point<int16_t>(52, 145));
        mp_label.draw(position + Point<int16_t>(52, 245));
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
        }
        return { Cursor::IDLE, false };
    }

    void UIPetOptions::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
            deactivate();
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

    void UIPetOptions::sync_labels()
    {
        hp_label.change_text("HP 低于或等于 " + std::to_string(hp_threshold) + "% 时补给");
        mp_label.change_text("MP 低于或等于 " + std::to_string(mp_threshold) + "% 时补给");
        hp_fill.setwidth(static_cast<int16_t>(SLIDER_WIDTH * hp_threshold / 100));
        mp_fill.setwidth(static_cast<int16_t>(SLIDER_WIDTH * mp_threshold / 100));
    }

    Rectangle<int16_t> UIPetOptions::slider_bounds(bool hp) const
    {
        int16_t y = hp ? HP_SLIDER_Y : MP_SLIDER_Y;
        return {
            position + Point<int16_t>(SLIDER_LEFT, static_cast<int16_t>(y - 8)),
            position + Point<int16_t>(SLIDER_LEFT + SLIDER_WIDTH, static_cast<int16_t>(y + 16))
        };
    }
}
