#include "IO/UITypes/StatusBar/QuickSlotModel.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
    using jrc::Point;
    using jrc::quickslot::COUNT;
    using jrc::quickslot::State;

    int16_t at(int16_t x, int16_t y)
    {
        return jrc::quickslot::index_at(Point<int16_t>(x, y));
    }

    void verify_centers()
    {
        constexpr int16_t CENTER_X[4] = { 20, 54, 87, 124 };
        for (int16_t i = 0; i < COUNT; ++i)
        {
            const int16_t y = static_cast<int16_t>(i < 4 ? 24 : 59);
            assert(at(CENTER_X[i % 4], y) == i);
        }
    }

    void verify_boundaries()
    {
        // Low edges are inside, high edges and one pixel beyond are not.
        assert(at(3, 7) == 0);
        assert(at(36, 41) == 0);
        assert(at(37, 7) == 1);
        assert(at(140, 76) == 7);
        assert(at(2, 7) == -1);
        assert(at(141, 7) == -1);
        assert(at(20, 6) == -1);
        assert(at(20, 77) == -1);
    }

    void verify_invalid_indices()
    {
        State state;
        assert(jrc::quickslot::skill_at(state, -1) == 0);
        assert(jrc::quickslot::skill_at(state, COUNT) == 0);
        assert(!jrc::quickslot::assign(state, -1, 1001004));
        assert(!jrc::quickslot::assign(state, COUNT, 1001004));

        // Clearing an invalid index is a no-op, not a crash.
        jrc::quickslot::clear(state, -1);
        jrc::quickslot::clear(state, COUNT);
        for (int16_t i = 0; i < COUNT; ++i)
        {
            assert(jrc::quickslot::skill_at(state, i) == 0);
        }
    }

    void verify_assign_and_clear()
    {
        State state;
        assert(state.hovered == -1);
        assert(state.press_id == 0);

        assert(jrc::quickslot::assign(state, 3, 1001004));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        // Bad skill ids are rejected and leave the stored value untouched.
        assert(!jrc::quickslot::assign(state, 3, 0));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);
        assert(!jrc::quickslot::assign(state, 3, -5));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        // Slots are independent.
        assert(jrc::quickslot::assign(state, 5, 2001002));
        assert(jrc::quickslot::skill_at(state, 5) == 2001002);
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        jrc::quickslot::clear(state, 3);
        assert(jrc::quickslot::skill_at(state, 3) == 0);
        assert(jrc::quickslot::skill_at(state, 5) == 2001002);

        jrc::quickslot::clear(state, 5);
        assert(jrc::quickslot::skill_at(state, 5) == 0);
    }
}

int main()
{
    verify_centers();
    verify_boundaries();
    verify_invalid_indices();
    verify_assign_and_clear();

    std::cout << "PASS quickslot model (hit bands, assign, clear)\n";
    return 0;
}
