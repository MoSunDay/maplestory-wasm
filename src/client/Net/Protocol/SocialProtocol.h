#pragma once

#include "../../IO/Social/SocialState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    class InPacket;
}

namespace jrc::social_protocol
{
    enum class BuddyEventType
    {
        UNKNOWN,
        REPLACE,
        REQUEST,
        CHANNEL,
        CAPACITY
    };

    struct BuddyEvent
    {
        BuddyEventType type = BuddyEventType::UNKNOWN;
        std::vector<social::BuddyEntry> entries;
        int32_t id = -1;
        int32_t channel = -1;
        uint8_t capacity = 0;
        std::string name;
    };

    BuddyEvent parse_buddy_event(InPacket& packet);
    std::vector<int8_t> encode_buddy_add(const std::string& name, const std::string& group);
    std::vector<int8_t> encode_buddy_id(uint8_t operation, int32_t id);
}
