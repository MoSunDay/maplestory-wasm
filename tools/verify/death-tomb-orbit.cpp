#include <cassert>
#include <cmath>

#include "Character/Effects/DeathTombOrbit.h"

int main()
{
    constexpr float PI = 3.14159265359f;
    using namespace jrc::death_orbit;

    assert(offset(0.0f) == jrc::Point<int16_t>(0, -30));
    assert(offset(PI / 2.0f) == jrc::Point<int16_t>(32, -18));
    assert(offset(PI) == jrc::Point<int16_t>(0, -6));
    assert(offset(PI * 1.5f) == jrc::Point<int16_t>(-32, -18));
    assert(!in_front(0.0f));
    assert(in_front(PI));
}
