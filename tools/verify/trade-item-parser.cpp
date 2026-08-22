#include "Net/Protocol/TradeProtocol.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using Bytes = std::vector<int8_t>;

    template <typename T>
    void append(Bytes& bytes, T value)
    {
        for (size_t index = 0; index < sizeof(T); ++index)
        {
            bytes.push_back(static_cast<int8_t>(value));
            value >>= 8;
        }
    }

    void append_string(Bytes& bytes, const std::string& value)
    {
        append<int16_t>(bytes, static_cast<int16_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    jrc::trade_protocol::ItemSummary parse(Bytes bytes)
    {
        jrc::InPacket packet(bytes.data(), bytes.size());
        auto result = jrc::trade_protocol::parse_item(packet);
        assert(packet.length() == 0);
        return result;
    }
}

int main()
{
    Bytes stack;
    append<int8_t>(stack, 2);
    append<int32_t>(stack, 2000005);
    append<int8_t>(stack, 0);
    append<int64_t>(stack, -1);
    append<int16_t>(stack, 37);
    append_string(stack, "owner");
    append<int16_t>(stack, 0);
    auto stack_item = parse(stack);
    assert(stack_item.item_id == 2000005 && stack_item.quantity == 37);

    Bytes equip;
    append<int8_t>(equip, 1);
    append<int32_t>(equip, 1302000);
    append<int8_t>(equip, 0);
    append<int64_t>(equip, -1);
    equip.insert(equip.end(), 32, 0);
    append_string(equip, "");
    equip.insert(equip.end(), 32, 0);
    auto equip_item = parse(equip);
    assert(equip_item.item_id == 1302000 && equip_item.quantity == 1);

    Bytes pet;
    append<int8_t>(pet, 3);
    append<int32_t>(pet, 5000000);
    append<int8_t>(pet, 1);
    append<int64_t>(pet, 12345);
    append<int64_t>(pet, -1);
    pet.insert(pet.end(), 35, 0);
    auto pet_item = parse(pet);
    assert(pet_item.item_id == 5000000 && pet_item.quantity == 1);
}
