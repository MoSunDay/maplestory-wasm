// Standalone verifier for map hazard footprint math and hit lookup
// (MapHazards), the pass behind damaging map objects (地刺/陷阱).
//
// No graphics or nx data is needed: it links only the header inline units.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client -Isrc
//       tools/verify/map-hazards.cpp -o /tmp/map-hazards
//   /tmp/map-hazards
//
// Asserts:
//   * hazard_bounds mirrors the sprite draw rectangle for unflipped objs;
//   * flipped objs mirror around the anchor x while keeping the y extent;
//   * find_map_hazard_hit only reports footprints overlapping the player.

#include <iostream>
#include <string>
#include <vector>

#include "Gameplay/MapleMap/MapHazards.h"

namespace
{
    using jrc::MapHazard;
    using jrc::find_map_hazard_hit;
    using jrc::hazard_bounds;

    std::vector<std::string> failures;

    void expect(bool ok, const char* label)
    {
        if (ok)
        {
            return;
        }

        failures.push_back(label);
        std::cout << "FAIL  " << label << '\n';
    }

    bool rect_is(const jrc::Rectangle<int16_t>& r, int16_t l, int16_t rt,
        int16_t rr, int16_t b)
    {
        return r.l() == l && r.t() == rt && r.r() == rr
            && r.b() == b;
    }
}

int main()
{
    // 18x33 spike anchored bottom-center like Obj/trap.img/trap/nature/9.
    const jrc::Point<int16_t> pos{ 100, 500 };
    const jrc::Point<int16_t> origin{ 9, 33 };
    const jrc::Point<int16_t> dims{ 18, 33 };

    const auto plain = hazard_bounds(pos, false, origin, dims);
    expect(rect_is(plain, 91, 467, 109, 500),
        "plain bounds anchor the sprite above its position");

    const auto flipped = hazard_bounds(pos, true, origin, dims);
    expect(rect_is(flipped, 91, 467, 109, 500),
        "symmetric sprite flips onto itself");
    expect(flipped.t() == plain.t() && flipped.b() == plain.b(),
        "flip keeps the vertical extent");

    const auto offcenter = hazard_bounds(pos, true, jrc::Point<int16_t>{ 0, 33 }, dims);
    expect(rect_is(offcenter, 82, 467, 100, 500),
        "asymmetric flip mirrors around the anchor x");

    // Hit lookup against a player box centered on the feet position.
    const std::vector<MapHazard> hazards{
        { plain, 10 },
        { { 400, 460, 420, 500 }, 300 }
    };
    const jrc::Rectangle<int16_t> on_spikes{ 95, 105, 470, 500 };
    const MapHazard* hit = find_map_hazard_hit(hazards, on_spikes);
    expect(hit != nullptr && hit->damage == 10,
        "player standing on the spike footprint takes its damage");

    const jrc::Rectangle<int16_t> beside_spikes{ 150, 160, 470, 500 };
    expect(find_map_hazard_hit(hazards, beside_spikes) == nullptr,
        "player next to the spikes is not hit");

    const jrc::Rectangle<int16_t> above_spikes{ 95, 105, 400, 430 };
    expect(find_map_hazard_hit(hazards, above_spikes) == nullptr,
        "player jumping well above the spikes is not hit");

    if (failures.empty())
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }

    std::cout << failures.size() << " CHECK(S) FAILED\n";
    return 1;
}
