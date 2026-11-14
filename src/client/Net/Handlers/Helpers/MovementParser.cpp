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
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "MovementParser.h"

#include <iostream>

namespace jrc
{
    std::vector<Movement> MovementParser::parse_movements(InPacket& recv)
    {
        std::vector<Movement> movements;
        uint8_t length = recv.read_byte();
        for (uint8_t i = 0; i < length; ++i)
        {
            Movement fragment;
            fragment.command = recv.read_byte();
            bool enqueue = true;
            switch (fragment.command)
            {
            case 0:
            case 5:
            case 17:
                // Absolute move: x y xwobble ywobble fh state duration.
                fragment.type = Movement::_ABSOLUTE;
                fragment.xpos = recv.read_short();
                fragment.ypos = recv.read_short();
                fragment.lastx = recv.read_short();
                fragment.lasty = recv.read_short();
                fragment.fh = recv.read_short();
                fragment.newstate = recv.read_byte();
                fragment.duration = recv.read_short();
                break;
            case 1:
            case 2:
            case 6:
            case 12:
            case 13:
            case 16:
            case 18:
            case 19:
            case 20:
            case 22:
                // Relative move: x y state duration.
                fragment.type = Movement::_RELATIVE;
                fragment.xpos = recv.read_short();
                fragment.ypos = recv.read_short();
                fragment.newstate = recv.read_byte();
                fragment.duration = recv.read_short();
                break;
            case 3:
            case 4:
            case 7:
            case 8:
            case 9:
            case 11:
                // Teleport-shaped move: x y xwobble ywobble state. The
                // server reads no duration here, so replay it instantly.
                fragment.type = Movement::TELEPORT;
                fragment.xpos = recv.read_short();
                fragment.ypos = recv.read_short();
                fragment.lastx = recv.read_short();
                fragment.lasty = recv.read_short();
                fragment.newstate = recv.read_byte();
                fragment.duration = 0;
                break;
            case 15:
                // Jump down with footholds: x y xwobble ywobble fh origin
                // fh state duration. The origin foothold is not replayed.
                fragment.type = Movement::JUMPDOWN;
                fragment.xpos = recv.read_short();
                fragment.ypos = recv.read_short();
                fragment.lastx = recv.read_short();
                fragment.lasty = recv.read_short();
                fragment.fh = recv.read_short();
                recv.skip(2);
                fragment.newstate = recv.read_byte();
                fragment.duration = recv.read_short();
                break;
            case 14:
                // Jump down without footholds; the server drops it after
                // parsing, so it must not enter the replay either.
                recv.skip(9);
                enqueue = false;
                break;
            case 21:
                // Aran movement; the server skips and drops it.
                recv.skip(3);
                enqueue = false;
                break;
            case 10:
                // Equip change; looks are broadcast in a separate packet.
                recv.skip(1);
                enqueue = false;
                break;
            default:
                // Unknown command: the server drops the whole list when it
                // hits one, and the payload layout is unknowable, so the
                // batch cannot be resynced — log and truncate it all.
                std::cerr << "[MovementParser] Unknown movement command "
                          << static_cast<int>(fragment.command)
                          << "; dropping the whole fragment batch"
                          << std::endl;
                movements.clear();
                return movements;
            }
            if (enqueue)
            {
                movements.push_back(fragment);
            }
        }
        return movements;
    }
}
