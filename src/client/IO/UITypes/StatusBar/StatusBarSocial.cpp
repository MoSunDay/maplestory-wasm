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
// Social (party / buddy / chat) forwards of the status bar. Split out of
// UIStatusBar.cpp to keep that translation unit within its size budget.
#include "../UIStatusBar.h"

namespace jrc
{
    void UIStatusbar::send_chatline(const std::string& line, UIChatbar::LineType type)
    {
        chatbar.send_line(line, type);
    }

    bool UIStatusbar::chat_open() const
    {
        return chatbar.is_open();
    }

    Point<int16_t> UIStatusbar::chat_open_button() const
    {
        return chatbar.open_button_center();
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
}
