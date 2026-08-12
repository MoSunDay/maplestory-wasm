#include <cassert>

#include "Character/Effects/DeathTombGround.h"

int main()
{
    using jrc::Point;
    using namespace jrc::death_tomb_ground;

    assert(landing_anchor({ 100, 200 }, { 100, 199 }) == Point<int16_t>(100, 200));
    assert(landing_anchor({ 100, 120 }, { 100, 249 }) == Point<int16_t>(100, 250));
    assert(landing_anchor({ -30, -80 }, { -30, 299 }) == Point<int16_t>(-30, 300));

    const Point<int16_t> fixed_anchor{ 100, 250 };
    assert(absolute_position(fixed_anchor, -25.4, 10.6) == Point<int16_t>(75, 261));
    assert(absolute_position(fixed_anchor, -50.0, -100.0) == Point<int16_t>(50, 150));
}
