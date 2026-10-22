#include "IO/Trade/PlayerContextLayout.h"

#include <cassert>
#include <iostream>

int main()
{
    using namespace jrc::player_context_layout;

    static_assert(WIDTH == 150, "context menu width must remain stable");
    static_assert(HEADER_HEIGHT == 24, "header must not be clickable");
    static_assert(HEIGHT == 72, "menu must reserve two action rows");
    static_assert(action_at(0) == Action::NONE, "header is not an action");
    static_assert(action_at(23) == Action::NONE, "header is not an action");
    static_assert(action_at(24) == Action::TRADE, "first row is trade");
    static_assert(action_at(47) == Action::TRADE, "first row is trade");
    static_assert(action_at(48) == Action::PARTY, "second row is party invite");
    static_assert(action_at(71) == Action::PARTY, "second row is party invite");
    static_assert(action_at(72) == Action::NONE, "outside the menu is not an action");

    assert(TRADE_TEXT_Y >= HEADER_HEIGHT);
    assert(PARTY_TEXT_Y >= HEADER_HEIGHT + ROW_HEIGHT);
    assert(PARTY_TEXT_Y < HEIGHT);

    std::cout << "PASS player context menu exposes trade and party rows\n";
    return 0;
}
