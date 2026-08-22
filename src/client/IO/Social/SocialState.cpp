#include "SocialState.h"

#include <algorithm>
#include <cctype>

namespace jrc::social
{
    namespace
    {
        std::string ascii_lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        void sort_buddies(std::vector<BuddyEntry>& entries)
        {
            std::stable_sort(entries.begin(), entries.end(), [](const BuddyEntry& left, const BuddyEntry& right) {
                const bool left_online = left.channel >= 0;
                const bool right_online = right.channel >= 0;
                if (left_online != right_online)
                {
                    return left_online;
                }
                return ascii_lower(left.name) < ascii_lower(right.name);
            });
        }
    }

    BuddyState replace_buddies(const BuddyState& state, std::vector<BuddyEntry> entries)
    {
        BuddyState next = state;
        sort_buddies(entries);
        next.entries = std::move(entries);
        next.pending_add_name.clear();
        return next;
    }

    BuddyState request_buddy(const BuddyState& state, int32_t id, const std::string& name)
    {
        BuddyState next = state;
        next.pending = { id, name };
        return next;
    }

    BuddyState clear_buddy_request(const BuddyState& state)
    {
        BuddyState next = state;
        next.pending = {};
        return next;
    }

    BuddyState begin_buddy_add(const BuddyState& state, const std::string& name)
    {
        BuddyState next = state;
        next.pending_add_name = name;
        return next;
    }

    BuddyState update_buddy_channel(const BuddyState& state, int32_t id, int32_t channel)
    {
        BuddyState next = state;
        bool found = false;
        for (BuddyEntry& entry : next.entries)
        {
            if (entry.id == id)
            {
                entry.channel = channel;
                found = true;
                break;
            }
        }
        if (!found && !next.pending_add_name.empty())
        {
            next.entries.push_back({ id, next.pending_add_name, "Default Group", channel, 0 });
        }
        next.pending_add_name.clear();
        sort_buddies(next.entries);
        return next;
    }

    BuddyState remove_buddy(const BuddyState& state, int32_t id)
    {
        BuddyState next = state;
        next.entries.erase(
            std::remove_if(next.entries.begin(), next.entries.end(), [id](const BuddyEntry& entry) {
                return entry.id == id;
            }),
            next.entries.end()
        );
        if (next.pending.id == id)
        {
            next.pending = {};
        }
        return next;
    }

    BuddyState update_buddy_capacity(const BuddyState& state, uint8_t capacity)
    {
        BuddyState next = state;
        next.capacity = capacity;
        return next;
    }

    PartyState invite_to_party(const PartyState& state, int32_t id, const std::string& inviter)
    {
        PartyState next = state;
        next.pending_id = id;
        next.pending_inviter = inviter;
        return next;
    }

    PartyState clear_party_invite(const PartyState& state)
    {
        PartyState next = state;
        next.pending_id = -1;
        next.pending_inviter.clear();
        return next;
    }

    PartyState replace_party(const PartyState& state, int32_t id, int32_t leader_id,
                             std::vector<PartyMember> members)
    {
        PartyState next = state;
        next.id = id;
        next.leader_id = leader_id;
        next.members = std::move(members);
        if (next.pending_id == id)
        {
            next = clear_party_invite(next);
        }
        return next;
    }

    PartyState confirm_party_created(const PartyState& state, int32_t id)
    {
        if (state.id == id && (!state.members.empty() || state.leader_id > 0))
        {
            return state;
        }

        PartyState next = state;
        next.id = id;
        next.leader_id = -1;
        next.members.clear();
        return next;
    }

    PartyState clear_party(const PartyState& state)
    {
        PartyState next = state;
        next.id = -1;
        next.leader_id = -1;
        next.members.clear();
        return next;
    }

    PartyState update_party_leader(const PartyState& state, int32_t leader_id)
    {
        PartyState next = state;
        next.leader_id = leader_id;
        return next;
    }

    PartyState update_party_member_hp(const PartyState& state, int32_t id, int32_t hp, int32_t max_hp)
    {
        PartyState next = state;
        for (PartyMember& member : next.members)
        {
            if (member.id == id)
            {
                member.hp = hp;
                member.max_hp = max_hp;
                break;
            }
        }
        return next;
    }

}
