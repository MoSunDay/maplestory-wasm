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
        if (statusbar::prechaos())
        {
            initialize_panel_prechaos(menu, {
                { "Character Info", Action::STATS },
                { "Equip", Action::EQUIPS },
                { "Inventory", Action::ITEMS },
                { "Skills", Action::SKILLS },
                { "Quests", Action::NONE }
            });
            initialize_panel_prechaos(system, {
                { "Key Config", Action::KEY_CONFIG },
                { "Pet Options", Action::PET_OPTIONS },
                { "Game Options", Action::GAME_OPTIONS },
                { "Quit Game", Action::LOGOUT }
            });
            return;
        }

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
            { "BtGameOption", Action::PET_OPTIONS },
            { "BtSystemOption", Action::GAME_OPTIONS },
            { "BtKeySetting", Action::KEY_CONFIG },
            { "BtJoyPad", Action::NONE },
            { "BtOption", Action::NONE },
            { "BtMonsterLife", Action::NONE },
            { "BtRoomChange", Action::NONE },
            { "BtGameQuit", Action::LOGOUT }
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

    void StatusBarPopup::initialize_panel_prechaos(
        PanelData& panel, const std::vector<ItemSpec>& specs)
    {
        // v83 popup frame: ContextMenu t (top with tail), c (row body, drawn
        // stretched), s (bottom cap). Rows are plain centered text.
        nl::node frame = nl::nx::ui["UIWindow.img"]["ContextMenu"];
        panel.top = frame["t"];
        panel.middle = frame["c"];
        panel.bottom = frame["s"];

        const size_t rows = specs.size();
        panel.middle_height = static_cast<int16_t>(
            rows * statusbar::popup_row_height() - 13
        );
        panel.width = statusbar::popup_width();
        panel.height = static_cast<int16_t>(
            panel.top.height() + panel.middle_height
            + statusbar::popup_bottom_height()
        );

        panel.hover_row = ColorBox(
            static_cast<int16_t>(
                statusbar::popup_item_right() - statusbar::popup_item_left()
            ),
            statusbar::popup_row_height(),
            Geometry::WHITE,
            0.25f
        );
        panel.hover = -1;

        panel.items.reserve(specs.size());
        for (const ItemSpec& spec : specs)
        {
            panel.items.push_back({
                Text(Text::A11M, Text::CENTER, Text::WHITE, spec.label),
                spec.action
            });
        }
    }

    void StatusBarPopup::draw(float) const
    {
        if (!is_open())
        {
            return;
        }

        const PanelData& panel = active_data();
        if (statusbar::prechaos())
        {
            draw_prechaos(panel);
            return;
        }

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

    void StatusBarPopup::draw_prechaos(const PanelData& panel) const
    {
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

        const int16_t label_x = static_cast<int16_t>(panel.width / 2);
        for (size_t index = 0; index < panel.items.size(); ++index)
        {
            const int16_t row_y = static_cast<int16_t>(
                statusbar::popup_item_top()
                + index * statusbar::popup_row_height()
            );

            if (panel.hover == static_cast<int16_t>(index))
            {
                panel.hover_row.draw(
                    panel.position + Point<int16_t>(
                        statusbar::popup_item_left(), row_y
                    )
                );
            }

            panel.items[index].label.draw(
                panel.position + Point<int16_t>(label_x, row_y + 2)
            );
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
        if (statusbar::prechaos())
        {
            // Anchor the pointer tail of ContextMenu/t on the button's left
            // edge so the popup opens beside it and grows upwards.
            int16_t x = static_cast<int16_t>(anchor.l() - statusbar::popup_tail_x());
            x = std::max<int16_t>(
                0,
                std::min<int16_t>(x, Constants::viewwidth() - panel.width)
            );
            int16_t y = static_cast<int16_t>(anchor.t() - panel.height);
            panel.position = { x, y };
            return;
        }

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
        selection = Action::NONE;
        remove_cursor();
    }

    void StatusBarPopup::open(Panel panel, Action selected_action)
    {
        active_panel = panel;
        selection = selected_action;
        remove_cursor();

        for (Entry& entry : active_data().entries)
        {
            if (entry.action == selection && entry.button->is_active())
            {
                entry.button->set_state(Button::MOUSEOVER);
                break;
            }
        }

        if (statusbar::prechaos())
        {
            select_action(active_data(), selected_action);
        }
    }

    void StatusBarPopup::select_action(PanelData& panel, Action action)
    {
        panel.hover = -1;
        for (size_t index = 0; index < panel.items.size(); ++index)
        {
            if (panel.items[index].action == action)
            {
                panel.hover = static_cast<int16_t>(index);
                break;
            }
        }
    }

    void StatusBarPopup::close()
    {
        active_panel = Panel::NONE;
        selection = Action::NONE;
        remove_cursor();
    }

    bool StatusBarPopup::is_open() const
    {
        return active_panel != Panel::NONE;
    }

    StatusBarPopup::Action StatusBarPopup::selected_action() const
    {
        return is_open() ? selection : Action::NONE;
    }

    bool StatusBarPopup::contains(Point<int16_t> cursor_position) const
    {
        return is_open() && panel_bounds(active_data()).contains(cursor_position);
    }

    std::optional<Point<int16_t>> StatusBarPopup::action_center(Action action) const
    {
        if (!is_open())
            return std::nullopt;

        const PanelData& panel = active_data();
        if (statusbar::prechaos())
        {
            for (size_t index = 0; index < panel.items.size(); ++index)
            {
                if (panel.items[index].action != action)
                    continue;
                Rectangle<int16_t> bounds = item_bounds(panel, index);
                return Point<int16_t>(
                    static_cast<int16_t>(bounds.l() + bounds.width() / 2),
                    static_cast<int16_t>(bounds.t() + bounds.height() / 2)
                );
            }
            return std::nullopt;
        }

        for (const Entry& entry : panel.entries)
        {
            if (entry.action != action || !entry.button->is_active())
                continue;
            Rectangle<int16_t> bounds = entry.button->bounds(panel.position);
            return Point<int16_t>(
                static_cast<int16_t>(bounds.l() + bounds.width() / 2),
                static_cast<int16_t>(bounds.t() + bounds.height() / 2)
            );
        }
        return std::nullopt;
    }

    Rectangle<int16_t> StatusBarPopup::panel_bounds(const PanelData& panel)
    {
        return {
            panel.position,
            panel.position + Point<int16_t>(panel.width, panel.height)
        };
    }

    Rectangle<int16_t> StatusBarPopup::item_bounds(
        const PanelData& panel, size_t index)
    {
        const int16_t top = static_cast<int16_t>(
            panel.position.y() + statusbar::popup_item_top()
            + index * statusbar::popup_row_height()
        );
        return {
            static_cast<int16_t>(panel.position.x() + statusbar::popup_item_left()),
            static_cast<int16_t>(panel.position.x() + statusbar::popup_item_right()),
            top,
            static_cast<int16_t>(top + statusbar::popup_row_height())
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
        if (statusbar::prechaos())
        {
            for (size_t index = 0; index < panel.items.size(); ++index)
            {
                if (!item_bounds(panel, index).contains(cursor_position))
                    continue;

                if (pressed)
                {
                    Sound(Sound::BUTTONCLICK).play();
                    return { Cursor::CLICKING, panel.items[index].action };
                }

                panel.hover = static_cast<int16_t>(index);
                selection = panel.items[index].action;
                return { Cursor::CANCLICK, Action::NONE };
            }

            return { Cursor::IDLE, Action::NONE };
        }

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
                selection = entry.action;
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
        menu.hover = -1;
        system.hover = -1;
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
