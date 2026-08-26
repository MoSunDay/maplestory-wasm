#include "client/Gameplay/MobCombat/DamageEncoding.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    using jrc::damage_encoding::decode;
    using jrc::damage_encoding::encode;

    assert(encode(100, false) == 100);
    assert(encode(0, true) == 0);
    assert(encode(-1, true) == 0);
    assert(static_cast<uint32_t>(encode(100, true)) == 0x80000064u);

    auto critical = decode(encode(100, true));
    assert(critical.critical && critical.damage == 100);
    auto maximum = decode(encode(std::numeric_limits<int32_t>::max(), true));
    assert(maximum.critical && maximum.damage == std::numeric_limits<int32_t>::max());
    auto normal = decode(100);
    assert(!normal.critical && normal.damage == 100);

    std::cout << "damage encoding verification passed\n";
}
