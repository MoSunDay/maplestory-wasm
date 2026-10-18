#include "Net/Protocol/DropRemovalPayload.h"
#include "Net/InPacket.h"

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace
{
    using Bytes = std::vector<int8_t>;
    using Effect = jrc::drop_removal::Effect;

    template <typename T>
    void append(Bytes& bytes, T value)
    {
        using Unsigned = typename std::make_unsigned<T>::type;
        const auto encoded = static_cast<Unsigned>(value);
        for (size_t index = 0; index < sizeof(T); ++index)
        {
            bytes.push_back(static_cast<int8_t>(encoded >> (index * 8)));
        }
    }

    jrc::drop_removal::Payload parse(const Bytes& bytes)
    {
        jrc::InPacket packet(bytes.data(), bytes.size());
        const auto payload = jrc::drop_removal::parse_payload(packet);
        assert(!packet.available());
        return payload;
    }

    Bytes base(int8_t mode, int32_t oid)
    {
        Bytes bytes;
        append<int8_t>(bytes, mode);
        append<int32_t>(bytes, oid);
        return bytes;
    }

    void verify_modes_without_looter()
    {
        const auto expiry = parse(base(0, 700001));
        assert(expiry.mode == 0);
        assert(expiry.oid == 700001);
        assert(expiry.character_id == 0);
        assert(expiry.pet_index == -1);
        assert(expiry.effect == Effect::EXPIRY_FADE);
        assert(expiry.known_mode);

        const auto immediate = parse(base(1, 700002));
        assert(immediate.effect == Effect::IMMEDIATE_ERASE);
        assert(immediate.known_mode);
    }

    void verify_character_modes()
    {
        Bytes player = base(2, 700003);
        append<int32_t>(player, 1000001);
        const auto player_pickup = parse(player);
        assert(player_pickup.character_id == 1000001);
        assert(player_pickup.effect == Effect::PLAYER_PICKUP);

        Bytes exploded = base(4, 700004);
        append<int32_t>(exploded, 1000002);
        const auto explosion = parse(exploded);
        assert(explosion.character_id == 1000002);
        assert(explosion.effect == Effect::IMMEDIATE_ERASE);
    }

    void verify_pet_mode()
    {
        Bytes pet = base(5, 700005);
        append<int32_t>(pet, 1000003);
        append<int8_t>(pet, 2);
        const auto pickup = parse(pet);
        assert(pickup.character_id == 1000003);
        assert(pickup.pet_index == 2);
        assert(pickup.effect == Effect::PET_PICKUP);
        assert(pickup.known_mode);
    }

    void verify_malformed_known_modes_fail()
    {
        Bytes truncated = base(5, 700006);
        append<int32_t>(truncated, 1000004);
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

        Bytes trailing = base(2, 700007);
        append<int32_t>(trailing, 1000005);
        append<int8_t>(trailing, 0);
        rejected = false;
        try
        {
            (void)parse(trailing);
        }
        catch (const jrc::PacketError&)
        {
            rejected = true;
        }
        assert(rejected);
    }

    void verify_unknown_mode_converges()
    {
        Bytes unknown = base(99, 700008);
        append<int32_t>(unknown, 1000006);
        append<int16_t>(unknown, 42);
        const auto payload = parse(unknown);
        assert(payload.mode == 99);
        assert(payload.oid == 700008);
        assert(!payload.known_mode);
        assert(payload.effect == Effect::IMMEDIATE_ERASE);
    }
}

int main()
{
    verify_modes_without_looter();
    verify_character_modes();
    verify_pet_mode();
    verify_malformed_known_modes_fail();
    verify_unknown_mode_converges();
}
