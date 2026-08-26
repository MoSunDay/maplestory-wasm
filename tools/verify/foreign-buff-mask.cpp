#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Net/Handlers/Helpers/BuffMaskOrder.h"

enum class Stat : uint8_t
{
    FIRST,
    DUPLICATE,
    LAST
};

int main()
{
    assert(jrc::buff_mask::ordered_flags(0).empty());

    const uint64_t low = uint64_t{1} << 2;
    const uint64_t middle = uint64_t{1} << 31;
    const uint64_t high = uint64_t{1} << 63;
    const std::vector<uint64_t> expected{ low, middle, high };
    assert(jrc::buff_mask::ordered_flags(low | middle | high) == expected);

    const std::unordered_map<Stat, uint64_t> codes{
        { Stat::DUPLICATE, low }, { Stat::FIRST, low }, { Stat::LAST, high }
    };
    const auto selection = jrc::buff_mask::ordered_ids<Stat>(low | high, codes);
    assert(selection.unsupported == 0);
    assert((selection.ids == std::vector<Stat>{ Stat::FIRST, Stat::LAST }));

    const auto unsupported = jrc::buff_mask::ordered_ids<Stat>(middle, codes);
    assert(unsupported.ids.empty());
    assert(unsupported.unsupported == middle);
}
