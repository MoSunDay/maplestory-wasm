#include "Gameplay/Targeting/PlayerTargeting.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace
{
    using jrc::PlayerTarget;
    using jrc::Point;

    std::vector<PlayerTarget> players()
    {
        return {
            { 20, "Right", 30, { 240, 200 } },
            { 10, "Left", 20, { 160, 200 } }
        };
    }
}

int main()
{
    const auto left = jrc::pick_player_target(players(), { 160, 150 }, {});
    assert(left && left->id == 10 && left->name == "Left");

    const auto shifted = jrc::pick_player_target(players(), { 60, 150 }, { -100, 0 });
    assert(shifted && shifted->id == 10);

    const auto missing = jrc::pick_player_target(players(), { 100, 250 }, {});
    assert(!missing);

    std::vector<PlayerTarget> overlap = {
        { 30, "Far", 10, { 205, 200 } },
        { 40, "Near", 10, { 200, 200 } }
    };
    const auto nearest = jrc::pick_player_target(overlap, { 200, 164 }, {});
    assert(nearest && nearest->id == 40);

    std::reverse(overlap.begin(), overlap.end());
    const auto order_independent = jrc::pick_player_target(overlap, { 200, 164 }, {});
    assert(order_independent && order_independent->id == 40);
}
