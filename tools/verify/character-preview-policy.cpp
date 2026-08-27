#include "IO/CharacterSelect/PreviewLoadPolicy.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <vector>

int main()
{
    using jrc::character_preview::LoadState;
    using jrc::character_preview::next_index;
    using jrc::character_preview::resolved_count;
    using jrc::character_preview::should_announce_completion;

    std::vector<LoadState> states(12, LoadState::PENDING);
    assert(next_index(states, 10, 1, 8) == 10);

    states[10] = LoadState::FAILED;
    assert(resolved_count(states) == 1);
    assert(next_index(states, 10, 1, 8) == 8);
    states[8] = LoadState::READY;
    states[9] = LoadState::FAILED;
    states[11] = LoadState::READY;
    assert(resolved_count(states) == 4);
    assert(next_index(states, 10, 1, 8) == 0);

    states = {
        LoadState::READY,
        LoadState::FAILED,
        LoadState::READY
    };
    assert(resolved_count(states) == states.size());
    assert(!next_index(states, 1, 0, 8));
    assert(should_announce_completion(states, false));
    assert(!should_announce_completion(states, true));

    states.assign(3, LoadState::PENDING);
    assert(resolved_count(states) == 0);
    assert(!should_announce_completion(states, false));
    assert(next_index(states, 99, 0, 8) == 0);
    assert(next_index(
        states,
        99,
        std::numeric_limits<size_t>::max(),
        8
    ) == 0);

    states.clear();
    assert(should_announce_completion(states, false));
    assert(!should_announce_completion(states, true));

    std::cout << "PASS character preview loading and failure completion\n";
}
