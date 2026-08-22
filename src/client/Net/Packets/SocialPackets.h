#pragma once

#include "../OutPacket.h"
#include "../Protocol/SocialProtocol.h"

namespace jrc
{
    class BuddyModifyPacket : public OutPacket
    {
    protected:
        explicit BuddyModifyPacket(const std::vector<int8_t>& payload)
            : OutPacket(BUDDYLIST_MODIFY)
        {
            write_bytes(payload);
        }
    };

    class AddBuddyPacket : public BuddyModifyPacket
    {
    public:
        explicit AddBuddyPacket(const std::string& name)
            : BuddyModifyPacket(social_protocol::encode_buddy_add(name, "Default Group")) {}
    };

    class AcceptBuddyPacket : public BuddyModifyPacket
    {
    public:
        explicit AcceptBuddyPacket(int32_t id)
            : BuddyModifyPacket(social_protocol::encode_buddy_id(2, id)) {}
    };

    class DeleteBuddyPacket : public BuddyModifyPacket
    {
    public:
        explicit DeleteBuddyPacket(int32_t id)
            : BuddyModifyPacket(social_protocol::encode_buddy_id(3, id)) {}
    };
}
