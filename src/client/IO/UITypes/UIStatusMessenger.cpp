#include "UIStatusMessenger.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/SocialPackets.h"
#include "../../Constants.h"

#include "UIParty.h"
#include "UIStatusBar.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    StatusInfo::StatusInfo(const std::string& str, Text::Color color)
    {
        text = { Text::A12M, Text::RIGHT, color, str };
        shadow = { Text::A12M, Text::RIGHT, Text::BLACK, str };
        opacity.set(1.0f);
    }

    void StatusInfo::draw(Point<int16_t> position, float alpha) const
    {
        float interopc = opacity.get(alpha);
        shadow.draw({ position + Point<int16_t>(1, 1), interopc });
        text.draw({ position, interopc });
    }

    bool StatusInfo::update()
    {
        constexpr float FADE_STEP = Constants::TIMESTEP * 1.0f / FADE_DURATION;

        opacity -= FADE_STEP;
        return opacity.last() < FADE_STEP;
    }


    UIStatusMessenger::UIStatusMessenger()
        : UIElement(Point<int16_t>(), Point<int16_t>(INVITE_WIDTH, INVITE_HEIGHT), true),
          status_anchor(),
          screen_width(Constants::viewwidth()),
          screen_height(Constants::viewheight()),
          has_party_invite(false),
          pending_party_invite_id(-1),
          has_buddy_request(false),
          pending_buddy_id(-1),
          invite_background(INVITE_WIDTH, INVITE_HEIGHT, Geometry::BLACK, 0.82f),
          invite_header(INVITE_WIDTH, 22, Geometry::WHITE, 0.12f),
          invite_divider(INVITE_WIDTH - 24, Geometry::WHITE, 0.20f),
          invite_title(Text::A12B, Text::LEFT, Text::WHITE, "Party Invitation"),
          invite_message(Text::A11M, Text::LEFT, Text::YELLOW, "", static_cast<uint16_t>(INVITE_WIDTH - 24))
    {
        nl::node basic = nl::nx::ui["Basic.img"];
        buttons[BT_ACCEPT] = std::make_unique<MapleButton>(basic["BtOK4"], Point<int16_t>(INVITE_WIDTH - 138, INVITE_HEIGHT - 32));
        buttons[BT_DECLINE] = std::make_unique<MapleButton>(basic["BtCancel4"], Point<int16_t>(INVITE_WIDTH - 68, INVITE_HEIGHT - 32));

        button_labels[BT_ACCEPT] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Accept");
        button_labels[BT_DECLINE] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Decline");

        buttons[BT_ACCEPT]->set_active(false);
        buttons[BT_DECLINE]->set_active(false);

        update_layout();
    }

    void UIStatusMessenger::draw(float inter) const
    {
        Point<int16_t> infopos = status_anchor;
        for (const StatusInfo& info : statusinfos)
        {
            info.draw(infopos, inter);
            infopos.shift_y(-16);
        }

        if (has_party_invite || has_buddy_request)
        {
            draw_party_invite(inter);
        }
    }

    void UIStatusMessenger::update()
    {
        for (auto iter = statusinfos.begin(); iter != statusinfos.end();)
        {
            if (iter->update())
            {
                iter = statusinfos.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    void UIStatusMessenger::update_screen(int16_t new_width, int16_t new_height)
    {
        screen_width = new_width;
        screen_height = new_height;
        update_layout();
    }

    void UIStatusMessenger::show_status(Text::Color color, const std::string& message)
    {
        statusinfos.push_front({ message, color });

        if (statusinfos.size() > MAX_MESSAGES)
            statusinfos.pop_back();
    }

    void UIStatusMessenger::show_party_invite(int32_t party_id, const std::string& inviter)
    {
        pending_party_invite_id = party_id;
        pending_party_inviter = inviter;
        has_party_invite = true;
        refresh_prompt();
    }

    void UIStatusMessenger::clear_party_invite()
    {
        has_party_invite = false;
        pending_party_invite_id = -1;
        pending_party_inviter.clear();
        refresh_prompt();
    }

    void UIStatusMessenger::show_buddy_request(int32_t buddy_id, const std::string& requester)
    {
        has_buddy_request = true;
        pending_buddy_id = buddy_id;
        pending_buddy_name = requester;
        refresh_prompt();
    }

    void UIStatusMessenger::clear_buddy_request()
    {
        has_buddy_request = false;
        pending_buddy_id = -1;
        pending_buddy_name.clear();
        refresh_prompt();
    }

    void UIStatusMessenger::refresh_prompt()
    {
        const bool visible = has_buddy_request || has_party_invite;
        if (has_buddy_request)
        {
            invite_title.change_text("Buddy Request");
            const std::string requester = pending_buddy_name.empty() ? "Someone" : pending_buddy_name;
            invite_message.change_text(requester + " wants to add you as a buddy.");
        }
        else if (has_party_invite)
        {
            invite_title.change_text("Party Invitation");
            const std::string inviter = pending_party_inviter.empty() ? "Someone" : pending_party_inviter;
            invite_message.change_text(inviter + " invited you to join a party.");
        }
        else
        {
            invite_title.change_text("");
            invite_message.change_text("");
        }

        if (auto it = buttons.find(BT_ACCEPT); it != buttons.end() && it->second)
        {
            it->second->set_active(visible);
            it->second->set_state(Button::NORMAL);
        }

        if (auto it = buttons.find(BT_DECLINE); it != buttons.end() && it->second)
        {
            it->second->set_active(visible);
            it->second->set_state(Button::NORMAL);
        }

        update_layout();
    }

    bool UIStatusMessenger::is_in_range(Point<int16_t> cursorpos) const
    {
        if (!has_party_invite && !has_buddy_request)
        {
            return false;
        }

        for (const auto& iter : buttons)
        {
            if (!iter.second || !iter.second->is_active())
            {
                continue;
            }

            if (iter.second->bounds(position).contains(cursorpos))
            {
                return true;
            }
        }

        return false;
    }

    bool UIStatusMessenger::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        UIElement::remove_cursor(clicked, cursorpos);
        return false;
    }

    Button::State UIStatusMessenger::button_pressed(uint16_t buttonid)
    {
        if (!has_party_invite && !has_buddy_request)
        {
            return Button::NORMAL;
        }

        if (has_buddy_request)
        {
            const int32_t buddy_id = pending_buddy_id;
            const std::string buddy_name = pending_buddy_name;
            if (buttonid == BT_ACCEPT && buddy_id > 0)
            {
                AcceptBuddyPacket(buddy_id).dispatch();
            }
            else if (buttonid == BT_DECLINE && buddy_id > 0)
            {
                DeleteBuddyPacket(buddy_id).dispatch();
            }

            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                const std::string action = buttonid == BT_ACCEPT ? "Accepted " : "Declined ";
                statusbar->send_chatline("[Buddy] " + action + buddy_name + ".", UIChatbar::YELLOW);
                statusbar->clear_pending_buddy_request();
            }
            else
            {
                clear_buddy_request();
            }
            return Button::PRESSED;
        }

        std::string inviter = pending_party_inviter;
        if (buttonid == BT_ACCEPT)
        {
            if (pending_party_invite_id > 0)
            {
                JoinPartyPacket(pending_party_invite_id).dispatch();
            }

            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                statusbar->send_chatline("[Party] Sent invitation accept request.", UIChatbar::YELLOW);
                statusbar->clear_pending_party_invite();
            }
            else
            {
                clear_party_invite();
            }
        }
        else if (buttonid == BT_DECLINE)
        {
            if (!inviter.empty())
            {
                DenyPartyInvitePacket(inviter).dispatch();
            }

            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                if (!inviter.empty())
                {
                    statusbar->send_chatline("[Party] Declined invitation from " + inviter + ".", UIChatbar::YELLOW);
                }
                statusbar->clear_pending_party_invite();
            }
            else
            {
                clear_party_invite();
            }
        }

        if (auto party_window = UI::get().get_element<UIParty>())
        {
            party_window->clear_pending_party_invite();
        }

        return Button::PRESSED;
    }

    void UIStatusMessenger::update_layout()
    {
        int32_t invite_x = static_cast<int32_t>(screen_width) - INVITE_WIDTH - SCREEN_PADDING;
        int32_t invite_y = static_cast<int32_t>(screen_height) - STATUSBAR_HEIGHT - INVITE_HEIGHT - SCREEN_PADDING;

        position = Point<int16_t>(
            static_cast<int16_t>(std::max(invite_x, static_cast<int32_t>(SCREEN_PADDING))),
            static_cast<int16_t>(std::max(invite_y, static_cast<int32_t>(SCREEN_PADDING)))
        );

        int32_t status_x = std::max(
            static_cast<int32_t>(SCREEN_PADDING),
            static_cast<int32_t>(screen_width) - SCREEN_PADDING
        );
        int32_t status_y = (has_party_invite || has_buddy_request) ?
            static_cast<int32_t>(position.y()) - 10 :
            static_cast<int32_t>(screen_height) - 100;
        status_y = std::max(status_y, static_cast<int32_t>(SCREEN_PADDING + 16));

        status_anchor = {
            static_cast<int16_t>(status_x),
            static_cast<int16_t>(status_y)
        };
    }

    void UIStatusMessenger::draw_party_invite(float alpha) const
    {
        invite_background.draw(position);
        invite_header.draw(position);
        invite_divider.draw(position + Point<int16_t>(12, 46));

        invite_title.draw(position + Point<int16_t>(12, 7));
        invite_message.draw(position + Point<int16_t>(12, 28));

        draw_buttons(alpha);
        draw_button_labels();
    }

    void UIStatusMessenger::draw_button_labels() const
    {
        for (const auto& label : button_labels)
        {
            auto button_iter = buttons.find(label.first);
            if (button_iter == buttons.end() || !button_iter->second || !button_iter->second->is_active())
            {
                continue;
            }

            Rectangle<int16_t> bounds = button_iter->second->bounds(position);
            Point<int16_t> center(
                static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                static_cast<int16_t>((bounds.t() + bounds.b()) / 2 + 2)
            );
            label.second.draw(center);
        }
    }
}
