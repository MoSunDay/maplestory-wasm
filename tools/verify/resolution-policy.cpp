// c++ -std=c++17 -Isrc/client -Isrc tools/verify/resolution-policy.cpp src/client/IO/Display/Resolution.cpp -o /tmp/resolution-policy
#include "IO/Display/Resolution.h"

#include <cassert>
#include <iostream>
#include <limits>

namespace
{
    bool same(jrc::display::Resolution lhs, jrc::display::Resolution rhs)
    {
        return lhs.width == rhs.width && lhs.height == rhs.height;
    }
}

int main()
{
    using namespace jrc::display;

    // Defaults and preset order.
    assert(preset_count() == 2);
    assert(same(default_resolution(), {800, 600}));
    assert(same(preset(0), {800, 600}));
    assert(same(preset(1), {1280, 720}));
    assert(same(preset(2), default_resolution()));
    assert(same(preset(999), default_resolution()));
    assert(same(preset(std::numeric_limits<std::size_t>::max()),
        default_resolution()));

    // Index lookup by exact dimensions.
    assert(index_of(800, 600) == 0);
    assert(index_of(1280, 720) == 1);
    assert(index_of(0, 0) == 0);
    assert(index_of(801, 600) == 0);
    assert(index_of(800, 601) == 0);
    assert(index_of(1280, 719) == 0);
    assert(index_of(1279, 720) == 0);

    // Index normalization.
    assert(normalize_index(0) == 0);
    assert(normalize_index(1) == 1);
    assert(normalize_index(-1) == 0);
    assert(normalize_index(2) == 0);
    assert(normalize_index(1000) == 0);

    // Forward/backward cycles wrap in both directions.
    assert(next_index(0, 1) == 1);
    assert(next_index(1, 1) == 0);
    assert(next_index(0, -1) == 1);
    assert(next_index(1, -1) == 0);
    assert(next_index(0, 0) == 0);
    assert(next_index(0, 2) == 0);
    assert(next_index(1, 2) == 1);
    assert(next_index(0, -2) == 0);
    assert(next_index(0, 3) == 1);
    assert(next_index(99, 1) == 1);
    assert(next_index(99, -1) == 1);
    assert(next_index(std::numeric_limits<std::size_t>::max(), 1) == 1);

    // Only exact presets are valid.
    assert(valid({800, 600}));
    assert(valid({1280, 720}));
    assert(!valid({1024, 768}));
    assert(!valid({0, 0}));
    assert(!valid({800, 720}));

    // Labels are stable and invalid input falls back to the default.
    assert(label({800, 600}) == "800 x 600");
    assert(label({1280, 720}) == "1280 x 720");
    assert(label({1024, 768}) == "800 x 600");

    std::cout << "resolution-policy: ok\n";
    return 0;
}
