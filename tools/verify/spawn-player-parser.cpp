#include "Net/Protocol/SpawnPlayerHeader.h"

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

    void append_string(Bytes& bytes, const std::string& value)
    {
        append_number<uint16_t>(bytes, static_cast<uint16_t>(value.size()));
        for (char byte : value)
        {
            bytes.push_back(static_cast<int8_t>(byte));
        }
    }

    jrc::spawn_player::Header parse_fixture(uint16_t level, const std::string& name)
    {
        Bytes bytes;
        append_number<int32_t>(bytes, 1000001);
        append_number<uint16_t>(bytes, level);
        append_string(bytes, name);

        jrc::InPacket packet(bytes.data(), bytes.size());
        const auto header = jrc::spawn_player::parse_header(packet);
        assert(!packet.available());
        return header;
    }
}

int main()
{
    const auto normal = parse_fixture(42, u8"测试角色");
    assert(normal.cid == 1000001);
    assert(normal.level == 42);
    assert(normal.name == u8"测试角色");

    const auto widened = parse_fixture(300, "Level300");
    assert(widened.level == 300);
    assert(widened.name == "Level300");

    Bytes truncated;
    append_number<int32_t>(truncated, 1000002);
    append_number<uint16_t>(truncated, 30);
    append_number<uint16_t>(truncated, 8);
    truncated.push_back('A');

    jrc::InPacket packet(truncated.data(), truncated.size());
    bool rejected = false;
    try
    {
        (void)jrc::spawn_player::parse_header(packet);
    }
    catch (const jrc::PacketError&)
    {
        rejected = true;
    }
    assert(rejected);
}
