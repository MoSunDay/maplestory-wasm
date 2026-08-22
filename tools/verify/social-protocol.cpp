#include "../../src/client/IO/Social/SocialState.h"
#include "../../src/client/Net/InPacket.h"
#include "../../src/client/Net/PacketError.h"
#include "../../src/client/Net/Protocol/SocialProtocol.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void byte(std::vector<int8_t>& bytes, int value)
    {
        bytes.push_back(static_cast<int8_t>(value));
    }

    void integer(std::vector<int8_t>& bytes, int32_t value)
    {
        for (size_t index = 0; index < sizeof(value); ++index)
        {
            byte(bytes, value);
            value >>= 8;
        }
    }

    void maple_string(std::vector<int8_t>& bytes, const std::string& value)
    {
        byte(bytes, value.size());
        byte(bytes, value.size() >> 8);
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    void fixed(std::vector<int8_t>& bytes, const std::string& value, size_t length)
    {
        bytes.insert(bytes.end(), value.begin(), value.end());
        bytes.insert(bytes.end(), length - value.size(), 0);
    }

    void verify_buddy_list()
    {
        std::vector<int8_t> bytes;
        byte(bytes, 7);
        byte(bytes, 2);
        integer(bytes, 10);
        fixed(bytes, "Offline", 13);
        byte(bytes, 0);
        integer(bytes, -1);
        fixed(bytes, "Default Group", 13);
        integer(bytes, 100);
        integer(bytes, 20);
        fixed(bytes, "Online", 13);
        byte(bytes, 0);
        integer(bytes, 1);
        fixed(bytes, "Default Group", 13);
        integer(bytes, 200);
        integer(bytes, 0);
        integer(bytes, 0);

        jrc::InPacket packet(bytes.data(), bytes.size());
        const auto event = jrc::social_protocol::parse_buddy_event(packet);
        assert(event.type == jrc::social_protocol::BuddyEventType::REPLACE);
        assert(event.entries.size() == 2);
        assert(event.entries[0].name == "Offline" && event.entries[0].channel == -1);
        assert(event.entries[1].name == "Online" && event.entries[1].channel == 1);
        assert(packet.length() == 0);

        jrc::social::BuddyState state;
        state = jrc::social::replace_buddies(state, event.entries);
        assert(state.entries[0].id == 20);
        state = jrc::social::update_buddy_channel(state, 10, 0);
        assert(state.entries[0].id == 10);
        state = jrc::social::request_buddy(state, 30, "Pending");
        assert(state.pending.id == 30);
        state = jrc::social::remove_buddy(state, 30);
        assert(state.pending.id == -1);
        state = jrc::social::begin_buddy_add(state, "Optimistic");
        state = jrc::social::update_buddy_channel(state, 40, -1);
        assert(state.entries.back().id == 40 && state.entries.back().name == "Optimistic");
    }

    void verify_buddy_request_and_channel()
    {
        std::vector<int8_t> request;
        byte(request, 9);
        integer(request, 77);
        maple_string(request, "中文测");
        jrc::InPacket request_packet(request.data(), request.size());
        const auto request_event = jrc::social_protocol::parse_buddy_event(request_packet);
        assert(request_event.type == jrc::social_protocol::BuddyEventType::REQUEST);
        assert(request_event.id == 77 && request_event.name == "中文测");

        std::vector<int8_t> channel;
        byte(channel, 0x14);
        integer(channel, 77);
        byte(channel, 0);
        integer(channel, -1);
        jrc::InPacket channel_packet(channel.data(), channel.size());
        const auto channel_event = jrc::social_protocol::parse_buddy_event(channel_packet);
        assert(channel_event.type == jrc::social_protocol::BuddyEventType::CHANNEL);
        assert(channel_event.id == 77 && channel_event.channel == -1);

        bool truncated_failed = false;
        channel.pop_back();
        try
        {
            jrc::InPacket truncated(channel.data(), channel.size());
            jrc::social_protocol::parse_buddy_event(truncated);
        }
        catch (const jrc::PacketError&)
        {
            truncated_failed = true;
        }
        assert(truncated_failed);
    }

    void verify_outbound_payloads()
    {
        const auto add = jrc::social_protocol::encode_buddy_add("Buddy", "Default Group");
        assert(add[0] == 1 && add[1] == 5 && add[2] == 0);
        assert(add.size() == 1 + 2 + 5 + 2 + 13);
        const auto accept = jrc::social_protocol::encode_buddy_id(2, 0x12345678);
        assert(accept.size() == 5 && accept[0] == 2);
        assert(static_cast<uint8_t>(accept[1]) == 0x78 && static_cast<uint8_t>(accept[4]) == 0x12);
        const auto remove = jrc::social_protocol::encode_buddy_id(3, 42);
        assert(remove[0] == 3 && remove[1] == 42);
    }

    void verify_party_reducer()
    {
        jrc::social::PartyState state;
        std::vector<jrc::social::PartyMember> members = {
            { 1, "Leader", 0, 12, 0, 100, 80, 100 },
            { 2, "Member", 0, 11, 0, 100, 60, 100 }
        };
        state = jrc::social::replace_party(state, 900, 1, members);
        state = jrc::social::confirm_party_created(state, 900);
        assert(state.leader_id == 1 && state.members.size() == 2);
        state = jrc::social::update_party_leader(state, 2);
        state = jrc::social::update_party_member_hp(state, 2, 25, 100);
        assert(state.leader_id == 2 && state.members[1].hp == 25);
        state = jrc::social::clear_party(state);
        assert(state.id == -1 && state.members.empty());
    }
}

int main()
{
    verify_buddy_list();
    verify_buddy_request_and_channel();
    verify_outbound_payloads();
    verify_party_reducer();
    std::cout << "Social protocol verification passed\n";
}
