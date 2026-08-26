#pragma once

#include "../../IO/KeyAction.h"

namespace jrc::confused_input
{
    struct HeldDirection
    {
        bool active = false;
        KeyAction::Id mapped = KeyAction::NOACTION;
    };

    struct State
    {
        HeldDirection left;
        HeldDirection right;
    };

    struct Transition
    {
        State state;
        KeyAction::Id action;
    };

    constexpr bool is_horizontal(KeyAction::Id action)
    {
        return action == KeyAction::LEFT || action == KeyAction::RIGHT;
    }

    constexpr KeyAction::Id map_direction(KeyAction::Id action, bool confused)
    {
        if (!confused)
            return action;
        if (action == KeyAction::LEFT)
            return KeyAction::RIGHT;
        if (action == KeyAction::RIGHT)
            return KeyAction::LEFT;
        return action;
    }

    // Latch the mapping selected on key-down so a disease transition between
    // key-down and key-up cannot release a different logical direction.
    constexpr Transition transition(State state, KeyAction::Id action,
                                    bool down, bool confused)
    {
        if (!is_horizontal(action))
            return { state, action };

        HeldDirection& held = action == KeyAction::LEFT ? state.left : state.right;
        if (down)
        {
            if (!held.active)
            {
                held.active = true;
                held.mapped = map_direction(action, confused);
            }
            return { state, held.mapped };
        }

        const KeyAction::Id mapped = held.active
            ? held.mapped
            : map_direction(action, confused);
        held = {};
        return { state, mapped };
    }
}
