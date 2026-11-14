//////////////////////////////////////////////////////////////////////////////
// Chair/seat state handling for the local player. Extracted from Player.cpp
// to keep file sizes within the project limits. Pure relocation, no change.
//////////////////////////////////////////////////////////////////////////////
#include "Player.h"

#include "Net/Packets/GameplayPackets.h"

namespace jrc
{
    void Player::set_seat(Optional<const Seat> seat)
    {
        if (seat)
        {
            CancelChairPacket(seat->get_id()).dispatch();
            chair_id = 0;
            map_seat_id = seat->get_id();
            set_chair(0);
            set_position(seat->getpos());
            set_state(Char::SIT);
        }
    }

    void Player::set_recovery_rate(float rate)
    {
        recovery_rate = rate >= 0.0f ? rate : 0.0f;
        recovery_state = {};
    }

    void Player::set_item_chair(int32_t item_id)
    {
        chair_id = item_id;
        map_seat_id = -1;
        set_chair(item_id);
        set_state(Char::SIT);
    }

    void Player::apply_server_chair_state(bool sitting)
    {
        if (!sitting)
        {
            chair_id = 0;
            map_seat_id = -1;
            set_chair(0);
        }
        Char::set_state(sitting ? Char::SIT : Char::STAND);
    }

}
