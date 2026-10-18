#include "CharacterInfoProtocol.h"

#include "../PacketError.h"

#include <cstddef>
#include <utility>

namespace jrc::character_info
{
    namespace
    {
        uint8_t read_unsigned_byte(InPacket& packet)
        {
            return static_cast<uint8_t>(packet.read_byte());
        }

        void append_int(std::vector<int8_t>& bytes, int32_t value)
        {
            for (size_t index = 0; index < sizeof(value); ++index)
            {
                bytes.push_back(static_cast<int8_t>(value));
                value >>= 8;
            }
        }

        void require_finished(const InPacket& packet)
        {
            if (packet.available())
                throw PacketError("Unexpected trailing character-info data");
        }
    }

    CharacterInfoPayload parse_character_info(InPacket& packet)
    {
        CharacterInfoPayload result;
        result.character_id = packet.read_int();
        result.level = static_cast<uint16_t>(packet.read_short());
        result.job_id = static_cast<uint16_t>(packet.read_short());
        result.fame = packet.read_short();
        result.married = packet.read_byte() != 0;
        result.guild = packet.read_string();
        result.alliance = packet.read_string();
        packet.read_byte();

        uint8_t pet_marker = read_unsigned_byte(packet);
        while (pet_marker != 0)
        {
            if (result.pets.size() >= 3)
                throw PacketError("Character info contains more than three pets");
            PetInfo pet;
            pet.marker = pet_marker;
            pet.item_id = packet.read_int();
            pet.name = packet.read_string();
            pet.level = read_unsigned_byte(packet);
            pet.closeness = packet.read_short();
            pet.fullness = read_unsigned_byte(packet);
            packet.read_short();
            pet.equipment_id = packet.read_int();
            result.pets.push_back(std::move(pet));
            pet_marker = read_unsigned_byte(packet);
        }

        result.mount.marker = read_unsigned_byte(packet);
        if (result.mount.marker != 0)
        {
            result.mount.level = packet.read_int();
            result.mount.experience = packet.read_int();
            result.mount.tiredness = packet.read_int();
        }

        const uint8_t wishlist_count = read_unsigned_byte(packet);
        result.wishlist.reserve(wishlist_count);
        for (uint8_t index = 0; index < wishlist_count; ++index)
            result.wishlist.push_back(packet.read_int());

        result.monster_book_level = packet.read_int();
        result.normal_cards = packet.read_int();
        result.special_cards = packet.read_int();
        result.total_cards = packet.read_int();
        result.monster_book_cover = packet.read_int();
        result.medal_item_id = packet.read_int();

        const int16_t medal_count = packet.read_short();
        if (medal_count < 0 ||
            static_cast<size_t>(medal_count) > packet.length() / sizeof(int16_t))
        {
            throw PacketError("Invalid medal quest count in character info");
        }
        result.medal_quests.reserve(static_cast<size_t>(medal_count));
        for (int16_t index = 0; index < medal_count; ++index)
            result.medal_quests.push_back(packet.read_short());

        require_finished(packet);
        return result;
    }

    FameResponse parse_fame_response(InPacket& packet)
    {
        FameResponse result;
        const uint8_t raw_status = read_unsigned_byte(packet);
        if (raw_status > static_cast<uint8_t>(FameStatus::ERROR))
            throw PacketError("Unknown fame response status");
        result.status = static_cast<FameStatus>(raw_status);

        if (result.status == FameStatus::SUCCESS)
        {
            result.character_name = packet.read_string();
            result.increase = packet.read_byte() != 0;
            result.new_fame = packet.read_short();
            packet.read_short();
        }
        else if (result.status == FameStatus::RECEIVED)
        {
            result.character_name = packet.read_string();
            result.increase = packet.read_byte() != 0;
        }

        require_finished(packet);
        return result;
    }

    std::vector<int8_t> encode_character_info_request(int32_t character_id)
    {
        std::vector<int8_t> bytes;
        bytes.reserve(8);
        append_int(bytes, 0);
        append_int(bytes, character_id);
        return bytes;
    }

    std::vector<int8_t> encode_give_fame(int32_t character_id, bool increase)
    {
        std::vector<int8_t> bytes;
        bytes.reserve(5);
        append_int(bytes, character_id);
        bytes.push_back(increase ? 1 : 0);
        return bytes;
    }

    const char* fame_status_message(FameStatus status)
    {
        switch (status)
        {
        case FameStatus::SUCCESS: return "人气操作成功。";
        case FameStatus::INVALID_CHARACTER: return "找不到该角色。";
        case FameStatus::LEVEL_TOO_LOW: return "15级后才能评价人气。";
        case FameStatus::DAILY_LIMIT: return "今天已经评价过人气。";
        case FameStatus::MONTHLY_LIMIT: return "本月已经评价过该角色。";
        case FameStatus::RECEIVED: return "收到其他玩家的人气评价。";
        case FameStatus::ERROR: return "人气操作失败。";
        }
        return "人气操作失败。";
    }
}
