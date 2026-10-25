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
#pragma once
#include "../PacketHandler.h"

namespace jrc
{
    // Activate or deactivate a named field obstacle.
    // Opcode: FIELD_OBSTACLE_ONOFF(139)
    class FieldObstacleOnoffHandler : public PacketHandler
    {
    public:
        void handle(InPacket& recv) const override;
    };


    // Apply a batch of named field obstacle state changes.
    // Opcode: FIELD_OBSTACLE_ONOFF_LIST(140)
    class FieldObstacleListHandler : public PacketHandler
    {
    public:
        void handle(InPacket& recv) const override;
    };


    // Return every named field obstacle to its map default state.
    // Opcode: FIELD_OBSTACLE_ALL_RESET(141)
    class FieldObstacleResetHandler : public PacketHandler
    {
    public:
        void handle(InPacket& recv) const override;
    };


    // Show a script progress message in a top screen banner.
    // Opcode: SCRIPT_PROGRESS_MESSAGE(122)
    class ScriptProgressMessageHandler : public PacketHandler
    {
    public:
        void handle(InPacket& recv) const override;
    };
}
