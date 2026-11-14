//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
// Local player movement reporting: one absolute Movement fragment per fixed
// update step, dispatched only while it differs from the last sent fragment.
#pragma once
#include <algorithm>

#include "../Constants.h"
#include "../Gameplay/Movement.h"
#include "../Net/Packets/GameplayPackets.h"

namespace jrc
{
    // Upper bound for a sent fragment duration: remote players interpolate
    // a fragment over its duration, so anything larger would look stalled.
    constexpr uint16_t MOVE_DURATION_MAX = 250;

    // Clamp milliseconds elapsed since the previous packet into a sendable
    // fragment duration. The floor is one update step; the local player
    // always reports real elapsed time instead of a hardcoded instant.
    inline int16_t movement_duration(uint16_t elapsed_ms)
    {
        return static_cast<int16_t>(
            std::clamp(elapsed_ms, Constants::TIMESTEP, MOVE_DURATION_MAX));
    }

    // Advance the local player's movement reporting by one update step.
    // 'movesince' accumulates the milliseconds since the previous packet and
    // becomes the new fragment's duration, so remote players interpolate at
    // the sender's real pace instead of snapping to each endpoint. While
    // moving, a packet leaves every step, so the interval is one timestep;
    // steps without a packet are idle and reset the accumulator, meaning
    // moving again after standing still starts at a single tick rather than
    // a stale, long-gapped duration.
    inline void send_player_movement(const PhysicsObject& phobj,
        uint8_t stancebyte, uint8_t command, Movement& lastmove,
        uint16_t& movesince)
    {
        movesince += Constants::TIMESTEP;
        Movement newmove(
            phobj, stancebyte, command, movement_duration(movesince));
        if (lastmove.hasmoved(newmove))
        {
            MovePlayerPacket(newmove).dispatch();
            lastmove = newmove;
        }
        movesince = 0;
    }
}
