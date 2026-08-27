#include "IO/CharacterSelect/PreviewLoadPolicy.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

int main()
{
    using jrc::character_preview::next_index;

    std::vector<bool> ready(12, false);
    assert(next_index(ready, 10, 1, 8) == 10);

    ready[10] = true;
    assert(next_index(ready, 10, 1, 8) == 8);
    ready[8] = true;
    ready[9] = true;
    ready[11] = true;
    assert(next_index(ready, 10, 1, 8) == 0);

    ready.assign(3, true);
    assert(!next_index(ready, 1, 0, 8));

    ready.assign(3, false);
    assert(next_index(ready, 99, 0, 8) == 0);
    assert(next_index(
        ready,
        99,
        std::numeric_limits<size_t>::max(),
        8
    ) == 0);

    std::cout << "PASS character preview loading order\n";
}
