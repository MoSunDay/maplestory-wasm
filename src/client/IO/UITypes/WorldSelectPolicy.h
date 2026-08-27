#pragma once

#include <cstdint>

namespace jrc::world_select
{
    class RequestGate
    {
    public:
        bool try_begin()
        {
            if (pending)
            {
                return false;
            }

            pending = true;
            return true;
        }

        void recover()
        {
            pending = false;
        }

        bool is_pending() const
        {
            return pending;
        }

    private:
        bool pending = false;
    };

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
}
