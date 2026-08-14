#include <algorithm>
#include <cassert>

#include "Character/Look/Attack/RegularStances.h"

int main()
{
    using namespace jrc;

    const auto& sword = regular_attack::choices(1, false);
    assert(sword.size() == 5);
    assert(std::find(sword.begin(), sword.end(), Stance::STABO1) != sword.end());
    assert(std::find(sword.begin(), sword.end(), Stance::SWINGO3) != sword.end());

    const auto all_sword = regular_attack::all_choices(1);
    assert(all_sword.size() == 6);
    assert(std::find(all_sword.begin(), all_sword.end(), Stance::PRONESTAB) != all_sword.end());

    const auto all_bow = regular_attack::all_choices(3);
    assert(std::find(all_bow.begin(), all_bow.end(), Stance::SHOOT1) != all_bow.end());
    assert(std::find(all_bow.begin(), all_bow.end(), Stance::SWINGT1) != all_bow.end());
    assert(std::find(all_bow.begin(), all_bow.end(), Stance::SWINGT3) != all_bow.end());

    assert(regular_attack::choices(0, false).empty());
    assert(regular_attack::choices(10, false).empty());
}
