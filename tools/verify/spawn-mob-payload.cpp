#include "Net/Protocol/SpawnMobPayload.h"

#include <cassert>
#include <cstdint>
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

    void append_fixed_state(Bytes& bytes, bool make_real = false)
    {
        bytes.insert(bytes.end(), 15, 0);
        bytes.push_back(static_cast<int8_t>(0x88));
        bytes.insert(bytes.end(), make_real ? 12 : 6, 0);
    }

    void append_position(
        Bytes& bytes,
        int16_t x,
        int16_t y,
        int8_t stance,
        uint16_t foothold)
    {
        append_number<int16_t>(bytes, x);
        append_number<int16_t>(bytes, y);
        append_number<int8_t>(bytes, stance);
        append_number<int16_t>(bytes, 0);
        append_number<uint16_t>(bytes, foothold);
    }

    void append_standard_tail(Bytes& bytes, int8_t marker, int8_t team)
    {
        append_number<int8_t>(bytes, marker);
        append_number<int8_t>(bytes, team);
        append_number<int32_t>(bytes, 0);
    }

    jrc::spawn_mob::Payload parse(
        Bytes& bytes,
        jrc::spawn_mob::Layout layout = jrc::spawn_mob::Layout::STANDARD)
    {
        jrc::InPacket packet(bytes.data(), bytes.size());
        const auto payload = jrc::spawn_mob::parse_payload(packet, layout);
        assert(!packet.available());
        return payload;
    }

    void verify_regular_spawns()
    {
        Bytes first;
        append_fixed_state(first);
        append_position(first, 157, 185, 5, 135);
        append_standard_tail(first, -2, -1);

        const auto new_spawn = parse(first);
        assert(new_spawn.position == jrc::Point<int16_t>(157, 185));
        assert(new_spawn.stance == 5);
        assert(new_spawn.foothold == 135);
        assert(new_spawn.new_spawn);
        assert(new_spawn.team == -1);
        assert(new_spawn.effect == 0);
        assert(new_spawn.parent_oid == 0);

        Bytes second;
        append_fixed_state(second);
        append_position(second, -10, -616, 4, 63);
        append_standard_tail(second, -1, 2);

        const auto instant = parse(second);
        assert(instant.position == jrc::Point<int16_t>(-10, -616));
        assert(instant.foothold == 63);
        assert(!instant.new_spawn);
        assert(instant.team == 2);
    }

    void verify_spawn_effects()
    {
        Bytes ordinary;
        append_fixed_state(ordinary);
        append_position(ordinary, 433, -120, 5, 7);
        append_number<int8_t>(ordinary, 3);
        append_number<int8_t>(ordinary, 0);
        append_number<int16_t>(ordinary, 0);
        append_standard_tail(ordinary, -2, -1);

        const auto ordinary_effect = parse(ordinary);
        assert(ordinary_effect.effect == 3);
        assert(ordinary_effect.new_spawn);
        assert(ordinary_effect.team == -1);

        Bytes special;
        append_fixed_state(special);
        append_position(special, 833, 196, 5, 132);
        append_number<int8_t>(special, 15);
        append_number<int8_t>(special, 0);
        append_number<int16_t>(special, 0);
        append_number<int8_t>(special, 0);
        append_standard_tail(special, -2, 1);

        const auto effect = parse(special);
        assert(effect.position == jrc::Point<int16_t>(833, 196));
        assert(effect.effect == 15);
        assert(effect.new_spawn);
        assert(effect.team == 1);

        Bytes parent;
        append_fixed_state(parent);
        append_position(parent, 526, 185, 5, 128);
        append_number<int8_t>(parent, -3);
        append_number<int32_t>(parent, 100000001);
        append_number<int8_t>(parent, -1);
        append_number<int32_t>(parent, 0);

        const auto linked = parse(parent);
        assert(linked.parent_oid == 100000001);
        assert(!linked.new_spawn);
        assert(linked.team == -1);

        Bytes parent_effect;
        append_fixed_state(parent_effect);
        append_position(parent_effect, 650, -362, 5, 42);
        append_number<int8_t>(parent_effect, 6);
        append_number<int32_t>(parent_effect, 100000002);
        append_number<int8_t>(parent_effect, -1);
        append_number<int32_t>(parent_effect, 0);

        const auto linked_effect = parse(parent_effect);
        assert(linked_effect.effect == 6);
        assert(linked_effect.parent_oid == 100000002);
        assert(!linked_effect.new_spawn);
    }

    void verify_fake_and_make_real()
    {
        Bytes fake;
        append_fixed_state(fake);
        append_position(fake, 1126, 193, 5, 126);
        append_number<int16_t>(fake, -2);
        append_number<int8_t>(fake, -1);
        append_number<int32_t>(fake, 0);

        const auto hidden = parse(fake);
        assert(hidden.position == jrc::Point<int16_t>(1126, 193));
        assert(hidden.new_spawn);

        Bytes fake_effect;
        append_fixed_state(fake_effect);
        append_position(fake_effect, 1434, 194, 5, 124);
        append_number<int8_t>(fake_effect, 3);
        append_number<int8_t>(fake_effect, 0);
        append_number<int16_t>(fake_effect, 0);
        append_number<int16_t>(fake_effect, -2);
        append_number<int8_t>(fake_effect, -1);
        append_number<int32_t>(fake_effect, 0);

        const auto hidden_effect = parse(fake_effect);
        assert(hidden_effect.effect == 3);
        assert(hidden_effect.new_spawn);

        Bytes make_real;
        append_fixed_state(make_real, true);
        append_position(make_real, 1126, 193, 5, 126);
        append_number<int16_t>(make_real, -1);
        append_number<int32_t>(make_real, 0);

        const auto visible = parse(
            make_real,
            jrc::spawn_mob::Layout::MAKE_REAL
        );
        assert(visible.position == jrc::Point<int16_t>(1126, 193));
        assert(visible.foothold == 126);
        assert(!visible.new_spawn);
    }

    void verify_invalid_packets_fail()
    {
        Bytes truncated;
        append_fixed_state(truncated);
        append_position(truncated, 157, 185, 5, 135);
        append_number<int8_t>(truncated, -2);

        bool rejected = false;
        try
        {
            (void)parse(truncated);
        }
        catch (const jrc::PacketError&)
        {
            rejected = true;
        }
        assert(rejected);

        Bytes wrong_contract(16, 0);
        append_position(wrong_contract, 157, 185, 5, 135);
        append_standard_tail(wrong_contract, -2, -1);

        rejected = false;
        try
        {
            (void)parse(wrong_contract);
        }
        catch (const jrc::PacketError&)
        {
            rejected = true;
        }
        assert(rejected);
    }
}

int main()
{
    verify_regular_spawns();
    verify_spawn_effects();
    verify_fake_and_make_real();
    verify_invalid_packets_fail();
}
