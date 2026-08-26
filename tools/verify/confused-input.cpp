#include "client/Gameplay/Input/ConfusedInput.h"

#include <cassert>
#include <iostream>

int main()
{
    using jrc::KeyAction::LEFT;
    using jrc::KeyAction::RIGHT;
    using jrc::confused_input::State;
    using jrc::confused_input::transition;

    State state;
    auto event = transition(state, LEFT, true, true);
    assert(event.action == RIGHT);
    event = transition(event.state, LEFT, false, false);
    assert(event.action == RIGHT);
    assert(!event.state.left.active);

    event = transition(event.state, RIGHT, true, false);
    assert(event.action == RIGHT);
    event = transition(event.state, RIGHT, true, true);
    assert(event.action == RIGHT);
    event = transition(event.state, RIGHT, false, true);
    assert(event.action == RIGHT);

    event = transition(event.state, LEFT, true, false);
    assert(event.action == LEFT);
    event = transition(event.state, LEFT, false, true);
    assert(event.action == LEFT);

    std::cout << "confused input verification passed\n";
}
