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
#include "CosmeticHandlers.h"

#include "../../Gameplay/Stage.h"

namespace jrc
{
    void ChalkboardHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();

        // v83 layout: cid, then 0 to close (packet ends) or 1 followed
        // by a maple ASCII string with the board text.
        int8_t open = recv.read_byte();
        if (open == 0)
        {
            // Routed through MapChars so a close arriving while the cid is
            // still queued for instantiation is stashed instead of dropped.
            Stage::get().get_chars().set_chalkboard(cid, "");
            return;
        }

        if (open == 1 && recv.length() >= 2)
        {
            const std::string text = recv.read_string();
            Stage::get().get_chars().set_chalkboard(cid, text);
        }
    }
}
