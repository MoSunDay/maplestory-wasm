#pragma once

#include <cstdint>

namespace jrc::drop_removal
{
    enum class Effect
    {
        EXPIRY_FADE,
        IMMEDIATE_ERASE,
        PLAYER_PICKUP,
        PET_PICKUP
    };

    struct Policy
    {
        Effect effect;
        bool has_character_id;
        bool has_pet_index;
        bool known_mode;
    };

    constexpr Policy policy_for_mode(int8_t mode)
    {
        switch (mode)
        {
        case 0:
            return { Effect::EXPIRY_FADE, false, false, true };
        case 1:
            return { Effect::IMMEDIATE_ERASE, false, false, true };
        case 2:
            return { Effect::PLAYER_PICKUP, true, false, true };
        case 4:
            return { Effect::IMMEDIATE_ERASE, true, false, true };
        case 5:
            return { Effect::PET_PICKUP, true, true, true };
        default:
            return { Effect::IMMEDIATE_ERASE, false, false, false };
        }
    }
}
