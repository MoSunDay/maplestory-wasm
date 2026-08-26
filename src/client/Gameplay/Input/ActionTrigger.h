#pragma once

namespace jrc::action_trigger
{
    enum class ProneJumpAction
    {
        BLOCKED,
        JUMP_DOWN,
        JUMP_UP
    };

    inline bool pressed_once(bool down, bool already_down)
    {
        return down && !already_down;
    }

    constexpr ProneJumpAction prone_jump_action(bool attacking, bool enable_jump_down, bool holding_down)
    {
        if (attacking)
        {
            return ProneJumpAction::BLOCKED;
        }

        return enable_jump_down && holding_down
            ? ProneJumpAction::JUMP_DOWN
            : ProneJumpAction::JUMP_UP;
    }
}
