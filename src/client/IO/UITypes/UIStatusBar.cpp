//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UIStatusBar.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "Pet/UIPetOptions.h"
#include "Settings/UIGameOptions.h"

#include "../../Character/ExpTable.h"
#include "../../Console.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"
#include "../../Net/Packets/LoginPackets.h"
#include "../../Net/Session.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <string>


namespace jrc
{
    constexpr Point<int16_t> UIStatusbar::POSITION;
    constexpr Point<int16_t> UIStatusbar::DIMENSION;

    UIStatusbar::UIStatusbar(const CharStats& st)
        : UIElement(POSITION, DIMENSION), stats(st), chatbar(POSITION)
    {
        if (statusbar::prechaos())
        {
            initialize_prechaos(nl::nx::ui["StatusBar.img"]);
        }
        else
        {
            initialize_postchaos();
        }

        update_layout_position();
    }

    void UIStatusbar::initialize_postchaos()
    {
        nl::node mainbar = nl::nx::ui["StatusBar2.img"]["mainBar"];

        sprites.emplace_back(mainbar["backgrnd"]);
        sprites.emplace_back(mainbar["gaugeBackgrd"]);
        sprites.emplace_back(mainbar["notice"]);
        sprites.emplace_back(mainbar["lvBacktrnd"]);
        sprites.emplace_back(mainbar["lvCover"]);

        expbar = {
            mainbar.resolve("gauge/exp/0"),
            mainbar.resolve("gauge/exp/1"),
            mainbar.resolve("gauge/exp/2"),
            308, 0.0f
        };
        hpbar = {
            mainbar.resolve("gauge/hp/0"),
            mainbar.resolve("gauge/hp/1"),
            mainbar.resolve("gauge/hp/2"),
            137, 0.0f
        };
        mpbar = {
            mainbar.resolve("gauge/mp/0"),
            mainbar.resolve("gauge/mp/1"),
            mainbar.resolve("gauge/mp/2"),
            137, 0.0f
        };

        statset   = { mainbar["gauge"]["number"], Charset::RIGHT };
        levelset  = { mainbar["lvNumber"],        Charset::LEFT  };

        joblabel  = { Text::A11M, Text::LEFT, Text::YELLOW };
        namelabel = { Text::A13M, Text::LEFT, Text::WHITE  };

        buttons[BT_WHISPER]   = std::make_unique<MapleButton>(mainbar["BtChat"]);
        buttons[BT_CALLGM]    = std::make_unique<MapleButton>(mainbar["BtClaim"]);

        buttons[BT_CASHSHOP]  = std::make_unique<MapleButton>(mainbar["BtCashShop"]);
        buttons[BT_TRADE]     = std::make_unique<MapleButton>(mainbar["BtMTS"]);
        buttons[BT_MENU]      = std::make_unique<MapleButton>(mainbar["BtMenu"]);
        buttons[BT_OPTIONS]   = std::make_unique<MapleButton>(mainbar["BtSystem"]);

        buttons[BT_CHARACTER] = std::make_unique<MapleButton>(mainbar["BtCharacter"]);
        buttons[BT_STATS]     = std::make_unique<MapleButton>(mainbar["BtStat"]);
        buttons[BT_QUEST]     = std::make_unique<MapleButton>(mainbar["BtQuest"]);
        buttons[BT_INVENTORY] = std::make_unique<MapleButton>(mainbar["BtInven"]);
        buttons[BT_EQUIPS]    = std::make_unique<MapleButton>(mainbar["BtEquip"]);
        buttons[BT_SKILL]     = std::make_unique<MapleButton>(mainbar["BtSkill"]);
    }

    void UIStatusbar::initialize_prechaos(nl::node statusbar)
    {
        nl::node base = statusbar["base"];

        // v83 ships one 1280x71 strip anchored bottom-left.
        sprites.emplace_back(base["backgrnd"]);

        nl::node gauge = statusbar["gauge"];
        gauge_graduation = gauge["graduation"];
        gauge_fill = gauge["bar"];

        statset  = { gauge["number"], Charset::RIGHT };
        levelset = { gauge["number"], Charset::LEFT  };

        joblabel  = { Text::A11M, Text::LEFT, Text::BLACK };
        namelabel = { Text::A11M, Text::LEFT, Text::BLACK };

        gauge_covers[0] = { 1, 1, Geometry::BLACK, statusbar::gauge_empty_opacity() };
        gauge_covers[1] = { 1, 1, Geometry::BLACK, statusbar::gauge_empty_opacity() };
        gauge_covers[2] = { 1, 1, Geometry::BLACK, statusbar::gauge_empty_opacity() };

        quickslot_panel = base["quickSlot"];
        nl::node keyset = statusbar["key"];
        for (int16_t i = 0; i < 8; ++i)
        {
            quickslot_keys[i] = keyset[std::to_string(i)];
        }

        // Menu bar buttons, right-anchored to the view.
        buttons[BT_TRADE]     = std::make_unique<MapleButton>(
            statusbar["BtShort"], statusbar::big_button_x(0), statusbar::big_button_y());
        buttons[BT_CASHSHOP]  = std::make_unique<MapleButton>(
            statusbar["BtShop"], statusbar::big_button_x(1), statusbar::big_button_y());
        buttons[BT_MENU]      = std::make_unique<MapleButton>(
            statusbar["BtMenu"], statusbar::big_button_x(2), statusbar::big_button_y());

        // Menu shortcut icons left of the quickslot display.
        buttons[BT_CHARACTER] = std::make_unique<MapleButton>(
            statusbar["KeySet"], statusbar::icon_button_pos(0));
        buttons[BT_STATS]     = std::make_unique<MapleButton>(
            statusbar["StatKey"], statusbar::icon_button_pos(1));
        buttons[BT_INVENTORY] = std::make_unique<MapleButton>(
            statusbar["InvenKey"], statusbar::icon_button_pos(2));
        buttons[BT_EQUIPS]    = std::make_unique<MapleButton>(
            statusbar["EquipKey"], statusbar::icon_button_pos(3));
        buttons[BT_SKILL]     = std::make_unique<MapleButton>(
            statusbar["SkillKey"], statusbar::icon_button_pos(4));
        buttons[BT_QUEST]     = std::make_unique<MapleButton>(
            statusbar["QuickSlot"], statusbar::icon_button_pos(5));

        // Small chat row buttons.
        buttons[BT_WHISPER] = std::make_unique<MapleButton>(
            statusbar["BtWhisper"], statusbar::whisper_pos());
        buttons[BT_CALLGM]  = std::make_unique<MapleButton>(
            statusbar["BtClaim"], statusbar::claim_pos());
    }

    void UIStatusbar::ensure_stat_labels(bool prechaos) const
    {
        int16_t level = stats.get_stat(Maplestat::LEVEL);
        int16_t hp    = stats.get_stat(Maplestat::HP);
        int16_t mp    = stats.get_stat(Maplestat::MP);
        int32_t maxhp = stats.get_total(Equipstat::HP);
        int32_t maxmp = stats.get_total(Equipstat::MP);
        int64_t exp   = stats.get_exp();
        int16_t exppercent = static_cast<int16_t>(100.0f * getexppercent() + 0.5f);

        const int8_t format = prechaos ? 1 : 0;
        if (format == label_format && level == cached_level && hp == cached_hp
            && mp == cached_mp && maxhp == cached_maxhp && maxmp == cached_maxmp
            && exp == cached_exp && exppercent == cached_exppercent)
        {
            return;
        }

        label_format = format;
        cached_level = level;
        cached_hp = hp;
        cached_mp = mp;
        cached_maxhp = maxhp;
        cached_maxmp = maxmp;
        cached_exp = exp;
        cached_exppercent = exppercent;

        if (prechaos)
        {
            // The v83 number font maps brackets/slash/percent to their node
            // names, so the composed strings reuse those glyphs.
            hp_label  = "L" + std::to_string(hp) + "s" + std::to_string(maxhp) + "R";
            mp_label  = "L" + std::to_string(mp) + "s" + std::to_string(maxmp) + "R";
            exp_label = std::to_string(exp) + "L" + std::to_string(exppercent) + "pR";
        }
        else
        {
            std::string expstring = std::to_string(100 * getexppercent());
            exp_label = std::to_string(exp) + "[" + expstring.substr(0, expstring.find('.') + 3) + "%]";
            hp_label  = "[" + std::to_string(hp) + "/" + std::to_string(maxhp) + "]";
            mp_label  = "[" + std::to_string(mp) + "/" + std::to_string(maxmp) + "]";
        }

        level_label = std::to_string(level);
    }

    void UIStatusbar::draw(float alpha) const
    {
        UIElement::draw(alpha);

        if (statusbar::prechaos())
        {
            draw_prechaos();
            chatbar.draw(alpha);
            popup.draw(alpha);
            return;
        }

        expbar.draw(position + Point<int16_t>(-261, -15));
        hpbar.draw(position + Point<int16_t>(-261, -31));
        mpbar.draw(position + Point<int16_t>(-90, -31));

        ensure_stat_labels(statusbar::prechaos());

        statset.draw(exp_label, position + Point<int16_t>(47, -13));
        statset.draw(hp_label, position + Point<int16_t>(-124, -29));
        statset.draw(mp_label, position + Point<int16_t>(47, -29));
        levelset.draw(level_label, position + Point<int16_t>(-480, -24));

        joblabel.draw(position + Point<int16_t>(-435, -21));
        namelabel.draw(position + Point<int16_t>(-435, -36));

        chatbar.draw(alpha);
        popup.draw(alpha);
    }

    void UIStatusbar::draw_prechaos() const
    {
        const Point<int16_t> gauge_pos = position + statusbar::gauge_anchor();

        // gauge/bar carries the labels plus the 100% gradient fills. The
        // unfilled rest of each slot is dimmed to the empty-slot look of
        // gauge/graduation, which is drawn on top (frame, ticks, base).
        gauge_fill.draw(gauge_pos);

        auto draw_cover = [this, gauge_pos](statusbar::Gauge kind, float percent) {
            const Rectangle<int16_t> fill = statusbar::gauge_fill(kind);
            const int16_t width = fill.width();
            const int16_t filled = static_cast<int16_t>(percent * width + 0.5f);
            const int16_t empty = static_cast<int16_t>(width - filled);
            if (empty <= 0)
            {
                return;
            }

            const int16_t index = static_cast<int16_t>(kind);
            gauge_covers[index].setwidth(empty);
            gauge_covers[index].draw(gauge_pos + Point<int16_t>(
                static_cast<int16_t>(fill.l() + filled), fill.t()
            ));
        };

        draw_cover(statusbar::Gauge::HP, gethppercent());
        draw_cover(statusbar::Gauge::MP, getmppercent());
        draw_cover(statusbar::Gauge::EXP, getexppercent());

        gauge_graduation.draw(gauge_pos);

        ensure_stat_labels(true);

        statset.draw(
            hp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::HP),
                statusbar::gauge_number_y()
            )
        );
        statset.draw(
            mp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::MP),
                statusbar::gauge_number_y()
            )
        );
        statset.draw(
            exp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::EXP),
                statusbar::gauge_number_y()
            )
        );

        levelset.draw(level_label, position + statusbar::level_pos());
        namelabel.draw(position + statusbar::name_pos());
        joblabel.draw(position + statusbar::job_pos());

        // Quickslot display (base/quickSlot + key caps).
        const Point<int16_t> quickslot_pos = position + statusbar::quickslot_pos();
        quickslot_panel.draw(quickslot_pos);
        for (int16_t i = 0; i < 8; ++i)
        {
            const Point<int16_t> center = statusbar::quickslot_key_center(i);
            const Point<int16_t> size = quickslot_keys[i].get_dimensions();
            quickslot_keys[i].draw(quickslot_pos + center - Point<int16_t>(
                size.x() / 2, size.y() / 2
            ));
        }
    }

    void UIStatusbar::update()
    {
        update_layout_position();
        UIElement::update();

        chatbar.update();

        if (!statusbar::prechaos())
        {
            expbar.update(getexppercent());
            hpbar.update(gethppercent());
            mpbar.update(getmppercent());
        }

        namelabel.change_text(stats.get_name());
        joblabel.change_text(stats.get_jobname());

        for (auto iter : message_cooldowns)
        {
            iter.second -= Constants::TIMESTEP;
        }
        cashshop_request_cooldown = std::max(
            0, cashshop_request_cooldown - static_cast<int32_t>(Constants::TIMESTEP));
    }

    void UIStatusbar::update_layout_position()
    {
        if (statusbar::prechaos())
        {
            position = statusbar::bar_position();
            dimension = statusbar::bar_dimension();
            chatbar.set_position(position);

            // v83 has a single menu button; the system popup opens to its
            // left so both stay on screen.
            auto menu = buttons.find(BT_MENU);
            if (menu != buttons.end() && menu->second)
            {
                Rectangle<int16_t> menu_anchor = menu->second->bounds(position);
                Rectangle<int16_t> system_anchor(
                    static_cast<int16_t>(menu_anchor.l() - 108),
                    static_cast<int16_t>(menu_anchor.l() - 4),
                    menu_anchor.t(),
                    menu_anchor.b()
                );
                popup.update_anchors(menu_anchor, system_anchor);
            }
            return;
        }

        position = {
            POSITION.x(),
            static_cast<int16_t>(Constants::viewheight() - Constants::VIEWYOFFSET)
        };
        chatbar.set_position(position);

        auto menu = buttons.find(BT_MENU);
        auto options = buttons.find(BT_OPTIONS);
        if (menu != buttons.end() && menu->second
            && options != buttons.end() && options->second)
        {
            popup.update_anchors(
                menu->second->bounds(position),
                options->second->bounds(position)
            );
        }
    }

    void UIStatusbar::update_screen(int16_t, int16_t)
    {
        // The v83 chrome is right-anchored to the live view width, so the big
        // buttons and shortcut icons must follow every screen change.
        if (statusbar::prechaos())
        {
            constexpr uint16_t BIG_BUTTONS[3] = { BT_TRADE, BT_CASHSHOP, BT_MENU };
            constexpr uint16_t ICON_BUTTONS[6] = {
                BT_CHARACTER, BT_STATS, BT_INVENTORY,
                BT_EQUIPS, BT_SKILL, BT_QUEST
            };

            auto place = [this](uint16_t id, Point<int16_t> pos) {
                auto iter = buttons.find(id);
                if (iter != buttons.end() && iter->second)
                {
                    iter->second->set_position(pos);
                }
            };

            for (int16_t i = 0; i < 3; ++i)
            {
                place(BIG_BUTTONS[i], {
                    statusbar::big_button_x(i), statusbar::big_button_y()
                });
            }
            for (int16_t i = 0; i < 6; ++i)
            {
                place(ICON_BUTTONS[i], statusbar::icon_button_pos(i));
            }
        }

        // Refresh the popup anchors so the menu keeps opening beside its
        // moved button.
        update_layout_position();
    }

    Button::State UIStatusbar::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_MENU:
            popup.toggle(StatusBarPopup::Panel::MENU);
            return Button::NORMAL;
        case BT_OPTIONS:
            popup.toggle(StatusBarPopup::Panel::SYSTEM);
            return Button::NORMAL;
        case BT_STATS:
            popup.close();
            UI::get().send_menu(KeyAction::CHARSTATS);
            return Button::NORMAL;
        case BT_INVENTORY:
            popup.close();
            UI::get().send_menu(KeyAction::INVENTORY);
            return Button::NORMAL;
        case BT_EQUIPS:
            popup.close();
            UI::get().send_menu(KeyAction::EQUIPS);
            return Button::NORMAL;
        case BT_SKILL:
            popup.close();
            UI::get().send_menu(KeyAction::SKILLBOOK);
            return Button::NORMAL;
        case BT_CASHSHOP:
            request_cash_shop();
            return Button::NORMAL;
        default:
            return Button::PRESSED;
        }
    }

    bool UIStatusbar::is_in_range(Point<int16_t> cursorpos) const
    {
        if (statusbar::prechaos())
        {
            return statusbar::statusbar_range(position).contains(cursorpos)
                || chatbar.is_in_range(cursorpos)
                || popup.contains(cursorpos);
        }

        Rectangle<int16_t> bounds(
            position - Point<int16_t>(512, 84),
            position - Point<int16_t>(512, 84) + dimension
        );

        return bounds.contains(cursorpos) || chatbar.is_in_range(cursorpos) ||
            popup.contains(cursorpos);
    }

    bool UIStatusbar::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        popup.remove_cursor();
        if (clicked && !popup.contains(cursorpos))
        {
            popup.close();
        }

        if (chatbar.remove_cursor(clicked, cursorpos))
        {
            return true;
        }

        return UIElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIStatusbar::send_cursor(bool pressed, Point<int16_t> cursorpos)
    {
        if (popup.contains(cursorpos))
        {
            StatusBarPopup::CursorResult result = popup.send_cursor(pressed, cursorpos);
            if (result.action != StatusBarPopup::Action::NONE)
            {
                dispatch_popup_action(result.action);
            }
            return { result.state, true };
        }

        bool toggles_popup =
            buttons[BT_MENU]->bounds(position).contains(cursorpos);
        if (const auto options = buttons.find(BT_OPTIONS); options != buttons.end())
        {
            toggles_popup = toggles_popup
                || options->second->bounds(position).contains(cursorpos);
        }
        if (pressed && popup.is_open() && !toggles_popup)
        {
            popup.close();
        }

        if (chatbar.is_in_range(cursorpos))
        {
            if (CursorResult child_result = chatbar.send_cursor(pressed, cursorpos))
            {
                return child_result;
            }

            return UIElement::send_cursor(pressed, cursorpos);
        }

        chatbar.send_cursor(pressed, cursorpos);
        return UIElement::send_cursor(pressed, cursorpos);
    }

    void UIStatusbar::toggle_menu_popup()
    {
        popup.toggle(StatusBarPopup::Panel::MENU);
    }

    void UIStatusbar::open_system_popup()
    {
        popup.open(StatusBarPopup::Panel::SYSTEM, StatusBarPopup::Action::LOGOUT);
    }

    bool UIStatusbar::activate_popup_selection()
    {
        const auto action = popup.selected_action();
        if (action == StatusBarPopup::Action::NONE)
        {
            return false;
        }

        dispatch_popup_action(action);
        return true;
    }

    void UIStatusbar::close_popup()
    {
        popup.close();
    }

    std::optional<Point<int16_t>> UIStatusbar::popup_action_center(
        StatusBarPopup::Action action) const
    {
        return popup.action_center(action);
    }

    void UIStatusbar::request_cash_shop()
    {
        if (cashshop_request_cooldown > 0)
            return;
        cashshop_request_cooldown = 2'000;
        popup.close();
        EnterCashShopPacket().dispatch();
    }

    void UIStatusbar::dispatch_popup_action(StatusBarPopup::Action action)
    {
        popup.close();
        switch (action)
        {
        case StatusBarPopup::Action::STATS:
            UI::get().send_menu(KeyAction::CHARSTATS);
            break;
        case StatusBarPopup::Action::EQUIPS:
            UI::get().send_menu(KeyAction::EQUIPS);
            break;
        case StatusBarPopup::Action::ITEMS:
            UI::get().send_menu(KeyAction::INVENTORY);
            break;
        case StatusBarPopup::Action::SKILLS:
            UI::get().send_menu(KeyAction::SKILLBOOK);
            break;
        case StatusBarPopup::Action::KEY_CONFIG:
            UI::get().send_menu(KeyAction::KEYCONFIG);
            break;
        case StatusBarPopup::Action::PET_OPTIONS:
            UI::get().emplace<UIPetOptions>();
            break;
        case StatusBarPopup::Action::GAME_OPTIONS:
            UI::get().emplace<UIGameOptions>();
            break;
        case StatusBarPopup::Action::LOGOUT:
        {
            PlayerDisconnectPacket().dispatch();
            Session::get().disconnect();
            const Error reconnect_error = Session::get().init();
            Stage::get().clear();
            UI::get().change_state(UI::LOGIN);
            UI::get().enable();
            if (reconnect_error != Error::NONE)
            {
                Console::get().print("Failed to reconnect to the login server after logout");
            }
            return;
        }
        case StatusBarPopup::Action::NONE:
            break;
        }
    }

    void UIStatusbar::send_chatline(const std::string& line, UIChatbar::LineType type)
    {
        chatbar.send_line(line, type);
    }

    void UIStatusbar::focus_chatfield()
    {
        chatbar.focus_chatfield();
    }

    void UIStatusbar::set_chat_target(UIChatbar::ChatTarget target)
    {
        chatbar.set_chat_target(target);
    }

    void UIStatusbar::cycle_chat_target()
    {
        chatbar.cycle_chat_target();
    }

    void UIStatusbar::set_pending_party_invite(int32_t party_id, const std::string& inviter)
    {
        chatbar.set_pending_party_invite(party_id, inviter);
    }

    void UIStatusbar::clear_pending_party_invite()
    {
        chatbar.clear_pending_party_invite();
    }

    void UIStatusbar::set_party_state(int32_t party_id, int32_t leader_id, const std::vector<UIChatbar::PartyMember>& members)
    {
        chatbar.set_party_state(party_id, leader_id, members);
    }

    void UIStatusbar::confirm_party_created(int32_t party_id)
    {
        chatbar.confirm_party_created(party_id);
    }

    void UIStatusbar::clear_party_state()
    {
        chatbar.clear_party_state();
    }

    void UIStatusbar::set_party_leader(int32_t leader_id)
    {
        chatbar.set_party_leader(leader_id);
    }

    void UIStatusbar::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
    {
        chatbar.update_party_member_hp(cid, hp, max_hp);
    }

    int32_t UIStatusbar::get_party_id() const
    {
        return chatbar.get_party_id();
    }

    int32_t UIStatusbar::get_party_leader_id() const
    {
        return chatbar.get_party_leader_id();
    }

    int32_t UIStatusbar::get_pending_party_invite_id() const
    {
        return chatbar.get_pending_party_invite_id();
    }

    const std::string& UIStatusbar::get_pending_party_inviter() const
    {
        return chatbar.get_pending_party_inviter();
    }

    const std::vector<UIChatbar::PartyMember>& UIStatusbar::get_party_members() const
    {
        return chatbar.get_party_members();
    }

    void UIStatusbar::set_buddy_list(const std::vector<social::BuddyEntry>& entries)
    {
        chatbar.set_buddy_list(entries);
    }

    void UIStatusbar::set_pending_buddy_request(int32_t id, const std::string& name)
    {
        chatbar.set_pending_buddy_request(id, name);
    }

    void UIStatusbar::clear_pending_buddy_request()
    {
        chatbar.clear_pending_buddy_request();
    }

    void UIStatusbar::remove_buddy(int32_t id)
    {
        chatbar.remove_buddy(id);
    }

    void UIStatusbar::begin_buddy_add(const std::string& name)
    {
        chatbar.begin_buddy_add(name);
    }

    void UIStatusbar::update_buddy_channel(int32_t id, int32_t channel)
    {
        chatbar.update_buddy_channel(id, channel);
    }

    void UIStatusbar::set_buddy_capacity(uint8_t capacity)
    {
        chatbar.set_buddy_capacity(capacity);
    }

    const social::BuddyState& UIStatusbar::get_buddy_state() const
    {
        return chatbar.get_buddy_state();
    }

    void UIStatusbar::record_multi_chat(uint8_t type)
    {
        chatbar.record_multi_chat(type);
    }

    uint32_t UIStatusbar::get_party_message_count() const
    {
        return chatbar.get_party_message_count();
    }

    void UIStatusbar::display_message(Messages::Type line, UIChatbar::LineType type)
    {
        if (message_cooldowns[line] > 0)
        {
            return;
        }

        std::string message{ Messages::messages[line] };
        chatbar.send_line(message, type);

        message_cooldowns[line] = MESSAGE_COOLDOWN;
    }

    float UIStatusbar::getexppercent() const
    {
        int16_t level = stats.get_stat(Maplestat::LEVEL);
        if (level >= ExpTable::LEVELCAP)
        {
            return 0.0f;
        }

        int64_t exp = stats.get_exp();
        return static_cast<float>(
            static_cast<double>(exp) / ExpTable::values[level]
        );
    }

    float UIStatusbar::gethppercent() const
    {
        int16_t hp = stats.get_stat(Maplestat::HP);
        int32_t maxhp = stats.get_total(Equipstat::HP);

        return static_cast<float>(hp) / maxhp;
    }

    float UIStatusbar::getmppercent() const
    {
        int16_t mp = stats.get_stat(Maplestat::MP);
        int32_t maxmp = stats.get_total(Equipstat::MP);

        return static_cast<float>(mp) / maxmp;
    }
}
