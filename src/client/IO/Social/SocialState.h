#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jrc::social
{
    struct BuddyEntry
    {
        int32_t id = 0;
        std::string name;
        std::string group;
        int32_t channel = -1;
        int32_t map_id = 0;
    };

    struct BuddyRequest
    {
        int32_t id = -1;
        std::string name;
    };

    struct BuddyState
    {
        std::vector<BuddyEntry> entries;
        BuddyRequest pending;
        std::string pending_add_name;
        uint8_t capacity = 0;
        uint32_t received_messages = 0;
    };

    struct PartyMember
    {
        int32_t id = 0;
        std::string name;
        int32_t job_id = 0;
        int32_t level = 0;
        int32_t channel = -2;
        int32_t map_id = 0;
        int32_t hp = 0;
        int32_t max_hp = 0;
    };

    struct PartyState
    {
        int32_t id = -1;
        int32_t leader_id = -1;
        int32_t pending_id = -1;
        std::string pending_inviter;
        std::vector<PartyMember> members;
        uint32_t received_messages = 0;
    };

    BuddyState replace_buddies(const BuddyState& state, std::vector<BuddyEntry> entries);
    BuddyState request_buddy(const BuddyState& state, int32_t id, const std::string& name);
    BuddyState clear_buddy_request(const BuddyState& state);
    BuddyState begin_buddy_add(const BuddyState& state, const std::string& name);
    BuddyState update_buddy_channel(const BuddyState& state, int32_t id, int32_t channel);
    BuddyState remove_buddy(const BuddyState& state, int32_t id);
    BuddyState update_buddy_capacity(const BuddyState& state, uint8_t capacity);

    PartyState invite_to_party(const PartyState& state, int32_t id, const std::string& inviter);
    PartyState clear_party_invite(const PartyState& state);
    PartyState replace_party(const PartyState& state, int32_t id, int32_t leader_id,
                             std::vector<PartyMember> members);
    PartyState confirm_party_created(const PartyState& state, int32_t id);
    PartyState clear_party(const PartyState& state);
    PartyState update_party_leader(const PartyState& state, int32_t leader_id);
    PartyState update_party_member_hp(const PartyState& state, int32_t id, int32_t hp, int32_t max_hp);
}
