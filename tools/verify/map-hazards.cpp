// Standalone verifier for map hazard footprint math and hit lookup
// (MapHazards), the pass behind damaging map objects (地刺/陷阱).
//
// Without arguments it links only the header inline units (no nx data). With
// a path to Map.nx it additionally runs the real collector over the Ellinia
// dungeon jet maps.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function
//       -Wno-deprecated-declarations -Isrc/client -Isrc -Isrc/nlnx
//       -Isrc/client/includes
//       tools/verify/map-hazards.cpp
//       src/client/Gameplay/MapleMap/MapHazards.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp
//       src/nlnx/bitmap.cpp src/nlnx/nx.cpp src/client/includes/lz4.c
//       -o /tmp/map-hazards
//   /tmp/map-hazards [assets/Map.nx]
//
// Asserts:
//   * hazard_bounds mirrors the sprite draw rectangle for unflipped objs;
//   * flipped objs mirror around the anchor x while keeping the y extent;
//   * hazard_bounds_from_hitbox places the authored lt/rb collision vectors;
//   * find_map_hazard_hit only reports footprints overlapping the player;
//   * with Map.nx: the steam jet maps (迷宫) collect hazards that hit.

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Gameplay/MapleMap/MapHazards.h"
#include "nlnx/file.hpp"
#include "nlnx/nx.hpp"

namespace
{
    using jrc::MapHazard;
    using jrc::find_map_hazard_hit;
    using jrc::hazard_bounds;
    using jrc::hazard_bounds_from_hitbox;

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

    // Real-data pass over the Ellinia dungeon steam jet maps (迷宫):
    // 105060000 and 990000620 place dozens of Obj/trap.img/steam/0/* jets
    // whose definitions carry `obstacle` but no `damage`.
    void check_nx_jets(const char* map_nx_path)
    {
        // The file must outlive every node derived from it: nl::node borrows
        // the mapped data without ownership.
        nl::file mapfile;
        try
        {
            mapfile.open(map_nx_path);
            nl::nx::map = mapfile.root();
        }
        catch (const std::exception& err)
        {
            expect(false, "Map.nx opens for the jet pass");
            std::cout << "  (" << err.what() << ")\n";
            return;
        }

        const std::vector<MapHazard> jets = jrc::collect_map_hazards(
            nl::nx::map["Map"]["Map1"]["105060000.img"]);
        expect(jets.size() >= 38,
            "map 105060000 collects its steam jets");
        bool all_damaging = !jets.empty();
        bool hits_player = false;
        const jrc::Rectangle<int16_t> player{ 707, 727, 482, 522 };
        for (const MapHazard& jet : jets)
        {
            if (jet.damage < 1)
                all_damaging = false;
            if (jet.bounds.overlaps(player))
                hits_player = true;
        }
        expect(all_damaging,
            "every jet of 105060000 deals damage (no-damage fallback)");
        expect(hits_player,
            "a jet of 105060000 overlaps the player box at (717, 522)");

        const std::vector<MapHazard> waterway = jrc::collect_map_hazards(
            nl::nx::map["Map"]["Map9"]["990000620.img"]);
        expect(waterway.size() >= 3,
            "map 990000620 collects its waterway jets");
    }
}

int main(int argc, char* argv[])
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

    // Authored lt/rb hit box of Obj/trap.img/steam/0/0 frame 0 (the full jet
    // column) placed at (717, 522) as in map 105060000.
    const jrc::Point<int16_t> jet_pos{ 717, 522 };
    const jrc::Point<int16_t> jet_lt{ -18, -68 };
    const jrc::Point<int16_t> jet_rb{ 16, 0 };

    const auto jet_plain = hazard_bounds_from_hitbox(
        jet_pos, false, jet_lt, jet_rb);
    expect(rect_is(jet_plain, 699, 454, 733, 522),
        "hit box spans the authored lt..rb column above the anchor");

    const auto jet_flipped = hazard_bounds_from_hitbox(
        jet_pos, true, jet_lt, jet_rb);
    expect(rect_is(jet_flipped, 701, 454, 735, 522),
        "flipped hit box mirrors around the anchor x");
    expect(jet_flipped.t() == jet_plain.t()
        && jet_flipped.b() == jet_plain.b(),
        "hit box flip keeps the vertical extent");

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

    if (argc > 1)
    {
        check_nx_jets(argv[1]);
    }

    if (failures.empty())
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }

    std::cout << failures.size() << " CHECK(S) FAILED\n";
    return 1;
}
