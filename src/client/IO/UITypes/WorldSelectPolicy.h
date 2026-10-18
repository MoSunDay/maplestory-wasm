#pragma once

#include <cstdint>

namespace jrc::world_select
{
    // The public client deliberately exposes one entry point into the game
    // world. Extra server channels are capacity details, not user choices.
    constexpr uint8_t selectable_channel_count(uint8_t server_channel_count)
    {
        return server_channel_count == 0 ? 0 : 1;
    }

    constexpr uint8_t selected_channel_id()
    {
        return 0;
    }

    // Charlist request gate, functional form of a recoverable in-flight flag.
    // One physical click can be observed by both the GLFW mouse handler and
    // the browser compatibility bridge, so the flag is raised before the
    // packet leaves; only an explicit recovery (failed charlist) clears it.
    inline bool begin_request(bool& pending)
    {
        if (pending)
        {
            return false;
        }

        pending = true;
        return true;
    }

    inline void recover_request(bool& pending)
    {
        pending = false;
    }
}
