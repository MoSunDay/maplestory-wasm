// c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Isrc/client -Isrc tools/verify/drop-pickup-selection.cpp -o /tmp/drop-pickup-selection && /tmp/drop-pickup-selection
#include "Gameplay/MapleMap/DropLifecycle/LootSelection.h"

#include <cstdio>
#include <vector>

using jrc::drop_loot::Candidate;
using jrc::drop_loot::select_nearest;
using jrc::Point;

#define CHECK(condition)                                                       \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                           \
                __FILE__, __LINE__, #condition);                              \
            return 1;                                                         \
        }                                                                     \
    } while (false)

namespace
{
    Candidate make_candidate(int32_t oid, int16_t x, int16_t y, bool lootable)
    {
        return { oid, Point<int16_t>(x, y), lootable };
    }
}

int main()
{
    const Point<int16_t> player(0, 0);

    // 1. Empty candidate list -> nothing reachable.
    {
        const std::vector<Candidate> candidates;
        CHECK(select_nearest(candidates, player, 0) == nullptr);
    }

    // 2. Nearest reachable lootable drop wins, regardless of vector order.
    {
        const std::vector<Candidate> candidates{
            make_candidate(2, 2, 2, true),   // center (18,18), dist^2 648
            make_candidate(1, 0, 0, true),   // center (16,16), dist^2 512
        };
        const Candidate* chosen = select_nearest(candidates, player, 0);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 1);
    }

    // 3. Drops beyond reach (outside the 32x32 bounds AND farther than
    // PICKUP_RADIUS from the center) are ignored.
    {
        const std::vector<Candidate> candidates{
            make_candidate(7, 50, 50, true), // center (66,66), dist^2 8712
        };
        CHECK(select_nearest(candidates, player, 0) == nullptr);
    }
    {
        // Same far drop plus one in reach: only the reachable one can win.
        const std::vector<Candidate> candidates{
            make_candidate(7, 50, 50, true),
            make_candidate(8, 0, 0, true),
        };
        const Candidate* chosen = select_nearest(candidates, player, 0);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 8);
    }

    // 4. Non-lootable drops never get selected, even when overlapping.
    {
        const std::vector<Candidate> candidates{
            make_candidate(9, 0, 0, false),
        };
        CHECK(select_nearest(candidates, player, 0) == nullptr);
    }
    {
        const std::vector<Candidate> candidates{
            make_candidate(9, 0, 0, false),
            make_candidate(10, 2, 2, true),
        };
        const Candidate* chosen = select_nearest(candidates, player, 0);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 10);
    }

    // 5. Overlap-only reach: drop at (0,0), player at (0,31). The center
    // (16,16) distance^2 is 16^2 + 15^2 = 481, so both the 30px radius and
    // the 32x32 bounds cover it; the bounds path is what keeps the whole
    // icon footprint pickable. It must be reachable.
    {
        const Point<int16_t> at_corner_edge(0, 31);
        const std::vector<Candidate> candidates{
            make_candidate(11, 0, 0, true),
        };
        const Candidate* chosen =
            select_nearest(candidates, at_corner_edge, 0);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 11);

        // Just past the bounds and beyond the radius: out of reach.
        const Point<int16_t> too_far(0, 45);
        CHECK(select_nearest(candidates, too_far, 0) == nullptr);
    }

    // 6. skip_oid only loses priority: the nearest drop is skipped in
    // favour of the next reachable one.
    {
        const std::vector<Candidate> candidates{
            make_candidate(1, 0, 0, true),   // nearest
            make_candidate(2, 2, 2, true),   // farther
        };
        const Candidate* chosen = select_nearest(candidates, player, 1);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 2);
    }

    // 7. skip_oid fallback: when the skipped oid is the only reachable
    // candidate it is still returned, so a server-side drop lock can be
    // retried instead of starving the pickup.
    {
        const std::vector<Candidate> candidates{
            make_candidate(1, 0, 0, true),
            make_candidate(2, 50, 50, true), // out of reach
        };
        const Candidate* chosen = select_nearest(candidates, player, 1);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 1);
    }

    // 8. Equal distance tie: the earlier candidate in the vector wins.
    {
        const Point<int16_t> centre(100, 100);
        const std::vector<Candidate> candidates{
            make_candidate(21, 100, 100, true), // center (116,116), 512
            make_candidate(22, 68, 68, true),   // center (84,84), 512
        };
        const Candidate* chosen = select_nearest(candidates, centre, 0);
        CHECK(chosen != nullptr);
        CHECK(chosen->oid == 21);

        const std::vector<Candidate> reordered{
            make_candidate(22, 68, 68, true),
            make_candidate(21, 100, 100, true),
        };
        const Candidate* chosen_reordered =
            select_nearest(reordered, centre, 0);
        CHECK(chosen_reordered != nullptr);
        CHECK(chosen_reordered->oid == 22);
    }

    std::printf("PASS drop pickup selection\n");
    return 0;
}
