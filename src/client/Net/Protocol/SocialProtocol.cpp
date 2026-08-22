#include "SocialProtocol.h"

#include "../InPacket.h"

namespace jrc::social_protocol
{
    namespace
    {
        void append_short(std::vector<int8_t>& bytes, uint16_t value)
        {
            bytes.push_back(static_cast<int8_t>(value));
            bytes.push_back(static_cast<int8_t>(value >> 8));
        }

        void append_int(std::vector<int8_t>& bytes, int32_t value)
        {
            for (size_t i = 0; i < sizeof(value); ++i)
            {
                bytes.push_back(static_cast<int8_t>(value));
                value >>= 8;
            }
        }

        void append_string(std::vector<int8_t>& bytes, const std::string& value)
        {
            append_short(bytes, static_cast<uint16_t>(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end());
        }
    }

    BuddyEvent parse_buddy_event(InPacket& packet)
    {
        BuddyEvent event;
        const auto mode = static_cast<uint8_t>(packet.read_byte());
        if (mode == 7)
        {
            event.type = BuddyEventType::REPLACE;
            const auto count = static_cast<uint8_t>(packet.read_byte());
            event.entries.reserve(count);
            for (uint8_t index = 0; index < count; ++index)
            {
                social::BuddyEntry entry;
                entry.id = packet.read_int();
                entry.name = packet.read_padded_string(13);
                packet.read_byte();
                entry.channel = packet.read_int();
                entry.group = packet.read_padded_string(13);
                entry.map_id = packet.read_int();
                event.entries.push_back(std::move(entry));
            }
            for (uint8_t index = 0; index < count; ++index)
            {
                packet.read_int();
            }
        }
        else if (mode == 9)
        {
            event.type = BuddyEventType::REQUEST;
            event.id = packet.read_int();
            event.name = packet.read_string();
        }
        else if (mode == 0x14)
        {
            event.type = BuddyEventType::CHANNEL;
            event.id = packet.read_int();
            packet.read_byte();
            event.channel = packet.read_int();
        }
        else if (mode == 0x15)
        {
            event.type = BuddyEventType::CAPACITY;
            event.capacity = static_cast<uint8_t>(packet.read_byte());
        }
        return event;
    }

    std::vector<int8_t> encode_buddy_add(const std::string& name, const std::string& group)
    {
        std::vector<int8_t> bytes = { 1 };
        append_string(bytes, name);
        append_string(bytes, group);
        return bytes;
    }

    std::vector<int8_t> encode_buddy_id(uint8_t operation, int32_t id)
    {
        std::vector<int8_t> bytes = { static_cast<int8_t>(operation) };
        append_int(bytes, id);
        return bytes;
    }
}
