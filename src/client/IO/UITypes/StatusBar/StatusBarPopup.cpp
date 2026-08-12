#include "StatusBarPopup.h"

#include "../../../Audio/Audio.h"
#include "../../../Constants.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        constexpr int16_t BUTTON_TOP = 9;
        constexpr int16_t BUTTON_HEIGHT = 25;
    }

    StatusBarPopup::StatusBarPopup()
    {
        nl::node main_bar = nl::nx::ui["StatusBar2.img"]["mainBar"];
        initialize_panel(menu, main_bar["Menu"], {
            { "BtStat", Action::STATS },
            { "BtEquip", Action::EQUIPS },
            { "BtItem", Action::ITEMS },
            { "BtSkill", Action::SKILLS },
            { "BtQuest", Action::NONE },
            { "BtCommunity", Action::NONE },
            { "BtEpisodBook", Action::NONE },
            { "BtEvent", Action::NONE },
            { "BtMonsterBattle", Action::NONE },
            { "BtMonsterLife", Action::NONE },
            { "BtBattleStats", Action::NONE },
            { "BtRank", Action::NONE },
            { "BtMSN", Action::NONE },
            { "BtAfreecaTV", Action::NONE }
        });
        initialize_panel(system, main_bar["System"], {
            { "BtChannel", Action::NONE },
            { "BtGameOption", Action::NONE },
            { "BtSystemOption", Action::NONE },
            { "BtKeySetting", Action::KEY_CONFIG },
            { "BtJoyPad", Action::NONE },
            { "BtOption", Action::NONE },
            { "BtMonsterLife", Action::NONE },
            { "BtRoomChange", Action::NONE },
            { "BtGameQuit", Action::QUIT }
        });
    }

    void StatusBarPopup::initialize_panel(PanelData& panel, nl::node source,
                                          const std::vector<ButtonSpec>& specs)
    {
        nl::node background = source["backgrnd"];
        panel.top = background["0"];
        panel.middle = background["1"];
        panel.bottom = background["2"];

        size_t middle_rows = specs.size() > 2 ? specs.size() - 2 : 0;
        panel.middle_height = static_cast<int16_t>(middle_rows * BUTTON_HEIGHT);
        panel.width = panel.top.width();
        panel.height = static_cast<int16_t>(
            panel.top.height() + panel.middle_height + panel.bottom.height()
        );

        panel.entries.reserve(specs.size());
        for (size_t index = 0; index < specs.size(); ++index)
        {
            const ButtonSpec& spec = specs[index];
            auto button = std::make_unique<MapleButton>(source[spec.node_name]);
            int16_t x = static_cast<int16_t>((panel.width - button->width()) / 2);
            int16_t y = static_cast<int16_t>(BUTTON_TOP + index * BUTTON_HEIGHT);
            button->set_position({ x, y });
            if (spec.action == Action::NONE)
            {
                button->set_state(Button::DISABLED);
            }
            panel.entries.push_back({ std::move(button), spec.action });
        }
    }

    void StatusBarPopup::draw(float) const
    {
        if (!is_open())
        {
            return;
        }

        const PanelData& panel = active_data();
        panel.top.draw(panel.position);
        panel.middle.draw(DrawArgument(
            panel.position + Point<int16_t>(0, panel.top.height()),
            Point<int16_t>(0, panel.middle_height)
        ));
        panel.bottom.draw(
            panel.position + Point<int16_t>(
                0,
                static_cast<int16_t>(panel.top.height() + panel.middle_height)
            )
        );

        for (const Entry& entry : panel.entries)
        {
            entry.button->draw(panel.position);
        }
    }

    void StatusBarPopup::update_anchors(Rectangle<int16_t> menu_anchor,
                                        Rectangle<int16_t> system_anchor)
    {
        position_panel(menu, menu_anchor);
        position_panel(system, system_anchor);
    }

    void StatusBarPopup::position_panel(PanelData& panel, Rectangle<int16_t> anchor)
    {
        int16_t x = static_cast<int16_t>(
            anchor.l() + (anchor.width() - panel.width) / 2
        );
        x = std::max<int16_t>(
            0,
            std::min<int16_t>(x, Constants::viewwidth() - panel.width)
        );
        int16_t y = static_cast<int16_t>(anchor.t() - panel.height);
        panel.position = { x, y };
    }

    void StatusBarPopup::toggle(Panel panel)
    {
        active_panel = active_panel == panel ? Panel::NONE : panel;
        remove_cursor();
    }

    void StatusBarPopup::close()
    {
        active_panel = Panel::NONE;
        remove_cursor();
    }

    bool StatusBarPopup::is_open() const
    {
        return active_panel != Panel::NONE;
    }

    bool StatusBarPopup::contains(Point<int16_t> cursor_position) const
    {
        return is_open() && panel_bounds(active_data()).contains(cursor_position);
    }

    Rectangle<int16_t> StatusBarPopup::panel_bounds(const PanelData& panel)
    {
        return {
            panel.position,
            panel.position + Point<int16_t>(panel.width, panel.height)
        };
    }

    StatusBarPopup::CursorResult StatusBarPopup::send_cursor(
        bool pressed,
        Point<int16_t> cursor_position)
    {
        if (!contains(cursor_position))
        {
            return {};
        }

        PanelData& panel = active_data();
        for (Entry& entry : panel.entries)
        {
            MapleButton& button = *entry.button;
            if (button.is_active() && button.bounds(panel.position).contains(cursor_position))
            {
                if (pressed)
                {
                    Sound(Sound::BUTTONCLICK).play();
                    return { Cursor::CLICKING, entry.action };
                }

                if (button.get_state() == Button::NORMAL)
                {
                    Sound(Sound::BUTTONOVER).play();
                    button.set_state(Button::MOUSEOVER);
                }
                return { Cursor::CANCLICK, Action::NONE };
            }

            if (button.get_state() == Button::MOUSEOVER)
            {
                button.set_state(Button::NORMAL);
            }
        }

        return { Cursor::IDLE, Action::NONE };
    }

    void StatusBarPopup::remove_cursor()
    {
        auto reset_panel = [](PanelData& panel) {
            for (Entry& entry : panel.entries)
            {
                if (entry.button->get_state() == Button::MOUSEOVER)
                {
                    entry.button->set_state(Button::NORMAL);
                }
            }
        };

        reset_panel(menu);
        reset_panel(system);
    }

    StatusBarPopup::PanelData& StatusBarPopup::active_data()
    {
        return active_panel == Panel::MENU ? menu : system;
    }

    const StatusBarPopup::PanelData& StatusBarPopup::active_data() const
    {
        return active_panel == Panel::MENU ? menu : system;
    }
}
