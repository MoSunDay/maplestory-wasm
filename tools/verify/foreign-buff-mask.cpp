#include <cassert>
#include <cstdint>
#include <vector>

#include "Net/Handlers/Helpers/ForeignBuffMask.h"

int main()
{
    assert(jrc::foreign_buff::ordered_flags(0).empty());

    const uint64_t low = uint64_t{1} << 2;
    const uint64_t middle = uint64_t{1} << 31;
    const uint64_t high = uint64_t{1} << 63;
    const std::vector<uint64_t> expected{ low, middle, high };
    assert(jrc::foreign_buff::ordered_flags(low | middle | high) == expected);
}
