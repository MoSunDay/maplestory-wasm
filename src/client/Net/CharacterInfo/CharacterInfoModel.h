#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jrc::character_info
{
    struct PetInfo
    {
        uint8_t marker = 0;
        int32_t item_id = 0;
        std::string name;
        uint8_t level = 0;
        int16_t closeness = 0;
        uint8_t fullness = 0;
        int32_t equipment_id = 0;
    };

    struct MountInfo
    {
        uint8_t marker = 0;
        int32_t level = 0;
        int32_t experience = 0;
        int32_t tiredness = 0;
    };

    struct CharacterInfoPayload
    {
        int32_t character_id = 0;
        uint16_t level = 0;
        uint16_t job_id = 0;
        int16_t fame = 0;
        bool married = false;
        std::string guild;
        std::string alliance;
        std::vector<PetInfo> pets;
        MountInfo mount;
        std::vector<int32_t> wishlist;
        int32_t monster_book_level = 0;
        int32_t normal_cards = 0;
        int32_t special_cards = 0;
        int32_t total_cards = 0;
        int32_t monster_book_cover = 0;
        int32_t medal_item_id = 0;
        std::vector<int16_t> medal_quests;
    };

    enum class FameStatus : uint8_t
    {
        SUCCESS = 0,
        INVALID_CHARACTER = 1,
        LEVEL_TOO_LOW = 2,
        DAILY_LIMIT = 3,
        MONTHLY_LIMIT = 4,
        RECEIVED = 5,
        ERROR = 6
    };

    struct FameResponse
    {
        FameStatus status = FameStatus::ERROR;
        std::string character_name;
        bool increase = true;
        int16_t new_fame = 0;
    };
}
