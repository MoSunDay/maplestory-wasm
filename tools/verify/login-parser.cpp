#include "Net/Handlers/Helpers/LoginParser.h"

#include <cassert>
#include <cstdint>
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

    void append_fixed_name(Bytes& bytes, const std::string& name,
        uint8_t utf16_units)
    {
        for (const char byte : name)
        {
            bytes.push_back(static_cast<int8_t>(byte));
        }
        for (uint8_t i = utf16_units; i < 13; ++i)
        {
            append_number<uint8_t>(bytes, 0);
        }
    }

    void append_stats(Bytes& bytes, const std::string& name,
        uint8_t name_units, uint16_t job, bool sp_table)
    {
        append_number<int32_t>(bytes, 1000001);
        append_fixed_name(bytes, name, name_units);
        append_number<uint8_t>(bytes, 0);
        append_number<uint8_t>(bytes, 1);
        append_number<int32_t>(bytes, 20000);
        append_number<int32_t>(bytes, 30000);
        append_number<int64_t>(bytes, 0);
        append_number<int64_t>(bytes, 0);
        append_number<int64_t>(bytes, 0);
        append_number<uint8_t>(bytes, 42);
        append_number<uint16_t>(bytes, job);
        append_number<uint16_t>(bytes, 12);
        append_number<uint16_t>(bytes, 13);
        append_number<uint16_t>(bytes, 14);
        append_number<uint16_t>(bytes, 15);
        append_number<uint16_t>(bytes, 1200);
        append_number<uint16_t>(bytes, 1300);
        append_number<uint16_t>(bytes, 800);
        append_number<uint16_t>(bytes, 900);
        append_number<uint16_t>(bytes, 7);
        if (sp_table)
        {
            append_number<uint8_t>(bytes, 2);
            append_number<uint8_t>(bytes, 1);
            append_number<uint8_t>(bytes, 3);
            append_number<uint8_t>(bytes, 2);
            append_number<uint8_t>(bytes, 4);
        }
        else
        {
            append_number<uint16_t>(bytes, 7);
        }
        append_number<int32_t>(bytes, 123456);
        append_number<int16_t>(bytes, 21);
        append_number<int32_t>(bytes, 0);
        append_number<int32_t>(bytes, 100000000);
        append_number<uint8_t>(bytes, 2);
        append_number<int32_t>(bytes, 0);
    }

    void append_look_and_rank(Bytes& bytes)
    {
        append_number<uint8_t>(bytes, 0);
        append_number<uint8_t>(bytes, 1);
        append_number<int32_t>(bytes, 20000);
        append_number<uint8_t>(bytes, 0);
        append_number<int32_t>(bytes, 30000);
        append_number<uint8_t>(bytes, 5);
        append_number<int32_t>(bytes, 1040002);
        append_number<uint8_t>(bytes, 0xFF);
        append_number<uint8_t>(bytes, 0xFF);
        append_number<int32_t>(bytes, 0);
        append_number<int32_t>(bytes, 0);
        append_number<int32_t>(bytes, 0);
        append_number<int32_t>(bytes, 0);
        append_number<uint8_t>(bytes, 0);
        append_number<uint8_t>(bytes, 1);
        append_number<int32_t>(bytes, 10);
        append_number<int32_t>(bytes, -2);
        append_number<int32_t>(bytes, 4);
        append_number<int32_t>(bytes, 1);
    }

    jrc::CharEntry parse_fixture(const std::string& name, uint8_t name_units,
        uint16_t job, bool sp_table)
    {
        Bytes bytes;
        append_stats(bytes, name, name_units, job, sp_table);
        append_look_and_rank(bytes);
        append_number<uint8_t>(bytes, 0);
        append_number<int32_t>(bytes, 6);

        jrc::InPacket packet(bytes.data(), bytes.size());
        auto character = jrc::LoginParser::parse_charentry(packet);
        assert(packet.read_byte() == 0);
        assert(packet.read_int() == 6);
        assert(!packet.available());
        return character;
    }
}

int main()
{
    const std::string chinese_name = u8"测试一";
    const auto character = parse_fixture(chinese_name, 3, 112, false);
    assert(character.cid == 1000001);
    assert(character.stats.name == chinese_name);
    assert(character.stats.stats[jrc::Maplestat::LEVEL] == 42);
    assert(character.stats.stats[jrc::Maplestat::JOB] == 112);
    assert(character.stats.stats[jrc::Maplestat::STR] == 12);
    assert(character.stats.stats[jrc::Maplestat::SP] == 7);
    assert(character.stats.mapid == 100000000);
    assert(character.stats.portal == 2);
    assert(character.look.equips.at(5) == 1040002);
    assert(character.stats.rank.first == 10);
    assert(character.stats.rank.second == '-');

    const std::string supplementary_name = u8"A😀中";
    const auto evan = parse_fixture(supplementary_name, 4, 2200, true);
    assert(evan.stats.name == supplementary_name);
    assert(evan.stats.stats[jrc::Maplestat::SP] == 7);
}
