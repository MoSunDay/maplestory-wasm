#include "Net/Packets/CharacterCreation/CreateCharPayload.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using jrc::CharacterCreation::encode_create_char_payload;

namespace
{
    uint16_t read_uint16(const std::vector<int8_t>& bytes, size_t& offset)
    {
        assert(offset + sizeof(uint16_t) <= bytes.size());
        uint16_t value = static_cast<uint8_t>(bytes[offset]);
        value |= static_cast<uint16_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8;
        offset += sizeof(uint16_t);
        return value;
    }

    int32_t read_int32(const std::vector<int8_t>& bytes, size_t& offset)
    {
        assert(offset + sizeof(int32_t) <= bytes.size());
        uint32_t value = 0;
        for (size_t index = 0; index < sizeof(int32_t); ++index)
        {
            value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + index])) <<
                (index * 8);
        }
        offset += sizeof(int32_t);
        return static_cast<int32_t>(value);
    }

    void verify_exact_wire_order()
    {
        const std::string name = "\xE6\xB5\x8B\xE8\xAF\x95\xE4\xB8\x80";
        const std::vector<int8_t> payload = encode_create_char_payload({
            name,
            1,
            20000,
            30030,
            7,
            3,
            1040002,
            1060002,
            1072001,
            1302000,
            1
        });

        assert(payload.size() == 2 + name.size() + 8 * sizeof(int32_t) + sizeof(int8_t));

        size_t offset = 0;
        const uint16_t name_length = read_uint16(payload, offset);
        assert(name_length == name.size());
        for (char byte : name)
        {
            assert(payload[offset++] == static_cast<int8_t>(byte));
        }

        assert(read_int32(payload, offset) == 1);
        assert(read_int32(payload, offset) == 20000);
        assert(read_int32(payload, offset) == 30037);
        assert(read_int32(payload, offset) == 3);
        assert(read_int32(payload, offset) == 1040002);
        assert(read_int32(payload, offset) == 1060002);
        assert(read_int32(payload, offset) == 1072001);
        assert(read_int32(payload, offset) == 1302000);
        assert(payload[offset++] == 1);
        assert(offset == payload.size());
    }

    void verify_gender_is_not_an_int()
    {
        const std::vector<int8_t> payload = encode_create_char_payload({
            "Hero123", 1, 20000, 30000, 0, 0, 1040002, 1060002, 1072001, 1302000, 0
        });
        assert(payload.size() == 2 + 7 + 8 * sizeof(int32_t) + sizeof(int8_t));
        assert(payload.back() == 0);
    }
}

int main()
{
    verify_exact_wire_order();
    verify_gender_is_not_an_int();
}
