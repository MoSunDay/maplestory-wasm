#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace jrc::CharacterCreation
{
    struct CreateCharPayload
    {
        std::string_view name;
        int32_t job;
        int32_t face;
        int32_t hair;
        int32_t hair_color;
        int32_t skin;
        int32_t top;
        int32_t bottom;
        int32_t shoes;
        int32_t weapon;
        int8_t gender;
    };

    inline std::vector<int8_t> encode_create_char_payload(const CreateCharPayload& payload)
    {
        std::vector<int8_t> bytes;
        bytes.reserve(2 + payload.name.size() + 8 * sizeof(int32_t) + sizeof(int8_t));

        auto append_little_endian = [&bytes](uint32_t value, size_t width)
        {
            for (size_t index = 0; index < width; ++index)
            {
                bytes.push_back(static_cast<int8_t>(value & 0xFF));
                value >>= 8;
            }
        };

        append_little_endian(static_cast<uint16_t>(payload.name.size()), sizeof(uint16_t));
        for (char byte : payload.name)
        {
            bytes.push_back(static_cast<int8_t>(byte));
        }

        // The linked login server currently runs with USE_CUSTOM_CLIENT and
        // reads a single packed hair integer on CREATE_CHAR.
        const int32_t fields[] = {
            payload.job,
            payload.face,
            payload.hair + payload.hair_color,
            payload.skin,
            payload.top,
            payload.bottom,
            payload.shoes,
            payload.weapon
        };
        for (int32_t field : fields)
        {
            append_little_endian(static_cast<uint32_t>(field), sizeof(field));
        }

        bytes.push_back(payload.gender);
        return bytes;
    }
}
