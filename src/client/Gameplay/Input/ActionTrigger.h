#pragma once

namespace jrc::action_trigger
{
    inline bool pressed_once(bool down, bool already_down)
    {
        return down && !already_down;
    }
}
