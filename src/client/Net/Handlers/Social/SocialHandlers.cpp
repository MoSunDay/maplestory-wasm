#include "SocialHandlers.h"

#include "../../../Gameplay/Stage.h"
#include "../../../IO/UI.h"
#include "../../../IO/UITypes/UIChatBar.h"
#include "../../../IO/UITypes/UIParty.h"
#include "../../../IO/UITypes/UIStatusBar.h"
#include "../../Protocol/SocialProtocol.h"

#include <array>

namespace jrc
{
    namespace
    {
        constexpr size_t PARTY_SIZE = 6;

        void read_party_status(InPacket& recv, int32_t& leader_id,
                               std::vector<UIChatbar::PartyMember>& members)
        {
            std::array<UIChatbar::PartyMember, PARTY_SIZE> raw;
            for (auto& member : raw) member.id = recv.read_int();
            for (auto& member : raw) member.name = recv.read_padded_string(13);
            for (auto& member : raw) member.job_id = recv.read_int();
            for (auto& member : raw) member.level = recv.read_int();
            for (auto& member : raw) member.channel = recv.read_int();
            leader_id = recv.read_int();
            for (auto& member : raw) member.map_id = recv.read_int();
            for (size_t index = 0; index < PARTY_SIZE; ++index)
            {
                recv.skip(sizeof(int32_t) * 4);
            }

            members.clear();
            for (const auto& member : raw)
            {
                if (member.id > 0) members.push_back(member);
            }
        }

        std::string party_status_message(int8_t mode, const std::string& name)
        {
            switch (mode)
            {
            case 1: case 5: case 6: case 11: case 14:
                return "[Party] Request failed due to an unexpected error.";
            case 10: return "[Party] Beginners cannot create a party.";
            case 12: return "[Party] You left as leader, so the party was disbanded.";
            case 13: return "[Party] You are not currently in a party.";
            case 16: return "[Party] That character is already in a party.";
            case 17: return "[Party] The party is already full.";
            case 19: return "[Party] Unable to find the requested character in this channel.";
            case 21: return "[Party] " + name + " is blocking party invitations.";
            case 22: return "[Party] " + name + " is handling another invitation.";
            case 23: return "[Party] " + name + " denied the party invitation.";
            case 25: return "[Party] You cannot kick that player in this map.";
            case 28: case 29:
                return "[Party] Leadership can only be transferred to nearby party members.";
            case 30: return "[Party] Leadership can only be transferred on the same channel.";
            default: return "";
            }
        }

        void update_party_views(UIStatusbar& statusbar, int32_t party_id,
                                int32_t leader_id, const std::vector<UIChatbar::PartyMember>& members)
        {
            statusbar.set_party_state(party_id, leader_id, members);
            Stage::get().get_chars().clear_party_member_hp();
            if (auto window = UI::get().get_element<UIParty>())
            {
                window->set_party_state(party_id, leader_id, members);
            }
        }

        void clear_party_views(UIStatusbar& statusbar)
        {
            statusbar.clear_party_state();
            Stage::get().get_chars().clear_party_member_hp();
            if (auto window = UI::get().get_element<UIParty>()) window->clear_party_state();
        }
    }

    void BuddyListHandler::handle(InPacket& recv) const
    {
        auto statusbar = UI::get().get_element<UIStatusbar>();
        if (!statusbar)
        {
            recv.skip(recv.length());
            return;
        }

        const social_protocol::BuddyEvent event = social_protocol::parse_buddy_event(recv);
        switch (event.type)
        {
        case social_protocol::BuddyEventType::REPLACE:
            statusbar->set_buddy_list(event.entries);
            break;
        case social_protocol::BuddyEventType::REQUEST:
            statusbar->set_pending_buddy_request(event.id, event.name);
            statusbar->send_chatline("[Buddy] " + event.name + " sent you a buddy request.", UIChatbar::YELLOW);
            break;
        case social_protocol::BuddyEventType::CHANNEL:
            statusbar->update_buddy_channel(event.id, event.channel);
            break;
        case social_protocol::BuddyEventType::CAPACITY:
            statusbar->set_buddy_capacity(event.capacity);
            break;
        case social_protocol::BuddyEventType::UNKNOWN:
            if (recv.available()) recv.skip(recv.length());
            break;
        }
    }

    void PartyOperationHandler::handle(InPacket& recv) const
    {
        const int8_t mode = recv.read_byte();
        auto statusbar = UI::get().get_element<UIStatusbar>();
        if (!statusbar)
        {
            recv.skip(recv.length());
            return;
        }

        if (mode == 4)
        {
            const int32_t party_id = recv.read_int();
            const std::string inviter = recv.read_string();
            recv.read_byte();
            statusbar->set_pending_party_invite(party_id, inviter);
            if (auto window = UI::get().get_element<UIParty>()) window->set_pending_party_invite(party_id, inviter);
            statusbar->send_chatline("[Party] " + inviter + " invited you. Use /party accept or /party deny.", UIChatbar::YELLOW);
            return;
        }
        if (mode == 7 || mode == 0x0F)
        {
            const int32_t party_id = recv.read_int();
            std::string joined_name;
            if (mode == 0x0F) joined_name = recv.read_string();
            int32_t leader_id = -1;
            std::vector<UIChatbar::PartyMember> members;
            read_party_status(recv, leader_id, members);
            update_party_views(*statusbar, party_id, leader_id, members);
            if (!joined_name.empty()) statusbar->send_chatline("[Party] " + joined_name + " joined the party.", UIChatbar::YELLOW);
            return;
        }
        if (mode == 8)
        {
            const int32_t party_id = recv.read_int();
            recv.skip(sizeof(int32_t) * 4);
            statusbar->confirm_party_created(party_id);
            if (auto window = UI::get().get_element<UIParty>())
            {
                window->set_party_state(statusbar->get_party_id(), statusbar->get_party_leader_id(), statusbar->get_party_members());
            }
            statusbar->send_chatline("[Party] Party created.", UIChatbar::YELLOW);
            return;
        }
        if (mode == 0x0C)
        {
            const int32_t party_id = recv.read_int();
            const int32_t target_id = recv.read_int();
            const int8_t disband = recv.read_byte();
            if (disband == 0)
            {
                recv.read_int();
                clear_party_views(*statusbar);
                statusbar->send_chatline("[Party] Party disbanded.", UIChatbar::YELLOW);
                return;
            }

            const bool expelled = recv.read_byte() != 0;
            const std::string target_name = recv.read_string();
            int32_t leader_id = -1;
            std::vector<UIChatbar::PartyMember> members;
            read_party_status(recv, leader_id, members);
            if (Stage::get().is_player(target_id)) clear_party_views(*statusbar);
            else update_party_views(*statusbar, party_id, leader_id, members);
            statusbar->send_chatline("[Party] " + target_name + (expelled ? " was expelled from" : " left") + " the party.", UIChatbar::YELLOW);
            return;
        }
        if (mode == 0x1B)
        {
            const int32_t leader_id = recv.read_int();
            recv.read_byte();
            statusbar->set_party_leader(leader_id);
            if (auto window = UI::get().get_element<UIParty>()) window->set_party_leader(leader_id);
            statusbar->send_chatline("[Party] Party leader changed.", UIChatbar::YELLOW);
            return;
        }
        if (mode == 0x23)
        {
            recv.skip(1 + sizeof(int32_t) * 2 + sizeof(int16_t) * 2);
            return;
        }

        std::string name;
        if (mode == 21 || mode == 22 || mode == 23) name = recv.read_string();
        const std::string message = party_status_message(mode, name);
        if (!message.empty()) statusbar->send_chatline(message, UIChatbar::YELLOW);
        else
        {
            statusbar->send_chatline("[Party] Unhandled operation code: " + std::to_string(mode), UIChatbar::RED);
            if (recv.available()) recv.skip(recv.length());
        }
    }

    void PartyValueHandler::handle(InPacket& recv) const
    {
        if (recv.available()) recv.skip(recv.length());
    }

    void UpdatePartyMemberHpHandler::handle(InPacket& recv) const
    {
        const int32_t id = recv.read_int();
        const int32_t hp = recv.read_int();
        const int32_t max_hp = recv.read_int();
        if (auto statusbar = UI::get().get_element<UIStatusbar>()) statusbar->update_party_member_hp(id, hp, max_hp);
        if (auto window = UI::get().get_element<UIParty>()) window->update_party_member_hp(id, hp, max_hp);
        Stage::get().get_chars().update_party_member_hp(id, hp, max_hp);
    }
}
