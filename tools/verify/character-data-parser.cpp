#include "Net/Handlers/Helpers/CharacterDataParser.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
    using Bytes = std::vector<int8_t>;

    template <typename T>
    void append_number(Bytes& bytes, T value)
    {
        using Unsigned = typename std::make_unsigned<T>::type;
        const auto encoded = static_cast<Unsigned>(value);
        for (size_t i = 0; i < sizeof(T); ++i)
        {
            bytes.push_back(static_cast<int8_t>(encoded >> (i * 8)));
        }
    }

    void append_string(Bytes& bytes, const std::string& value)
    {
        append_number<uint16_t>(bytes, static_cast<uint16_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    void append_fixed_utf8(Bytes& bytes, const std::string& value,
        uint16_t utf16_units, uint16_t field_units)
    {
        bytes.insert(bytes.end(), value.begin(), value.end());
        for (uint16_t i = utf16_units; i < field_units; ++i)
        {
            append_number<uint8_t>(bytes, 0);
        }
    }

    void expect_packet_error(const std::function<void()>& action)
    {
        bool threw = false;
        try
        {
            action();
        }
        catch (const jrc::PacketError&)
        {
            threw = true;
        }
        assert(threw);
    }

    void verify_fixed_utf8()
    {
        Bytes bytes;
        append_fixed_utf8(bytes, "ASCII", 5, 13);
        append_fixed_utf8(bytes, u8"测试一", 3, 13);
        append_fixed_utf8(bytes, u8"A😀中", 4, 13);
        append_fixed_utf8(bytes, "", 0, 13);
        append_fixed_utf8(bytes, std::string(13, 'x'), 13, 13);
        append_fixed_utf8(bytes, u8"礼物消息😀", 6, 73);
        append_fixed_utf8(bytes, std::string(73, 'y'), 73, 73);
        append_number<uint32_t>(bytes, 0x78563412);

        jrc::InPacket packet(bytes.data(), bytes.size());
        assert(packet.read_padded_utf8_string(13) == "ASCII");
        assert(packet.read_padded_utf8_string(13) == u8"测试一");
        assert(packet.read_padded_utf8_string(13) == u8"A😀中");
        assert(packet.read_padded_utf8_string(13).empty());
        assert(packet.read_padded_utf8_string(13) == std::string(13, 'x'));
        assert(packet.read_padded_utf8_string(73) == u8"礼物消息😀");
        assert(packet.read_padded_utf8_string(73) == std::string(73, 'y'));
        assert(static_cast<uint32_t>(packet.read_int()) == 0x78563412);
        assert(!packet.available());

        Bytes malformed = {
            static_cast<int8_t>(0xE0), static_cast<int8_t>(0x80),
            static_cast<int8_t>(0x80)
        };
        jrc::InPacket malformed_packet(malformed.data(), malformed.size());
        expect_packet_error([&]() {
            malformed_packet.read_padded_utf8_string(1);
        });

        Bytes truncated = {
            static_cast<int8_t>(0xE4), static_cast<int8_t>(0xB8)
        };
        jrc::InPacket truncated_packet(truncated.data(), truncated.size());
        expect_packet_error([&]() {
            truncated_packet.read_padded_utf8_string(1);
        });
    }

    void append_ring_info(Bytes& bytes)
    {
        append_number<int16_t>(bytes, 1);
        append_number<int32_t>(bytes, 11);
        append_fixed_utf8(bytes, u8"伙伴甲", 3, 13);
        for (int i = 0; i < 4; ++i) append_number<int32_t>(bytes, 20 + i);

        append_number<int16_t>(bytes, 1);
        append_number<int32_t>(bytes, 31);
        append_fixed_utf8(bytes, u8"A😀中", 4, 13);
        for (int i = 0; i < 5; ++i) append_number<int32_t>(bytes, 40 + i);

        append_number<int16_t>(bytes, 1);
        append_number<int32_t>(bytes, 51);
        append_number<int32_t>(bytes, 52);
        append_number<int32_t>(bytes, 53);
        append_number<int16_t>(bytes, 3);
        append_number<int32_t>(bytes, 54);
        append_number<int32_t>(bytes, 55);
        append_fixed_utf8(bytes, u8"新人甲", 3, 13);
        append_fixed_utf8(bytes, u8"新人乙", 3, 13);
    }

    void append_new_year_card(Bytes& bytes, int32_t id,
        const std::string& sender, const std::string& receiver)
    {
        append_number<int32_t>(bytes, id);
        append_number<int32_t>(bytes, id + 100);
        append_string(bytes, sender);
        append_number<uint8_t>(bytes, 0);
        append_number<int64_t>(bytes, 1000 + id);
        append_number<int32_t>(bytes, id + 200);
        append_string(bytes, receiver);
        append_number<uint8_t>(bytes, 0);
        append_number<uint8_t>(bytes, 1);
        append_number<int64_t>(bytes, 2000 + id);
        append_string(bytes, u8"新年快乐😀");
    }

    void verify_character_data()
    {
        Bytes bytes;
        append_number<int16_t>(bytes, 0);
        append_ring_info(bytes);
        append_number<int16_t>(bytes, 2);
        append_new_year_card(bytes, 1, u8"发送者甲", u8"接收者甲");
        append_new_year_card(bytes, 2, u8"发送者乙", u8"接收者乙");
        append_number<uint32_t>(bytes, 0x78563412);

        jrc::InPacket packet(bytes.data(), bytes.size());
        jrc::CharacterDataParser::parse_minigame_info(packet);
        jrc::CharacterDataParser::parse_ring_info(packet);
        jrc::CharacterDataParser::parse_new_year_info(packet);
        assert(static_cast<uint32_t>(packet.read_int()) == 0x78563412);
        assert(!packet.available());

        Bytes unsupported;
        append_number<int16_t>(unsupported, 1);
        jrc::InPacket unsupported_packet(unsupported.data(), unsupported.size());
        expect_packet_error([&]() {
            jrc::CharacterDataParser::parse_minigame_info(unsupported_packet);
        });

        Bytes negative;
        append_number<int16_t>(negative, -1);
        jrc::InPacket negative_packet(negative.data(), negative.size());
        expect_packet_error([&]() {
            jrc::CharacterDataParser::parse_new_year_info(negative_packet);
        });
    }
}

int main()
{
    verify_fixed_utf8();
    verify_character_data();
}
