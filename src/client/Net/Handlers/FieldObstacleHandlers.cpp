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
#include "FieldObstacleHandlers.h"

#include "../../Console.h"
#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIProgressBanner.h"

namespace
{
    // Apply one obstacle state change. Only logs when the named object is
    // missing so periodic toggles cannot spam the console.
    void apply_obstacle_state(const std::string& name, int32_t state)
    {
        const size_t changed =
            jrc::Stage::get().set_named_object_active(name, state == 1);
        if (changed == 0)
        {
            jrc::Console::get().print(
                "[FieldObstacleHandlers] Named field object not found: " + name +
                ", state: " + std::to_string(state) +
                ", map: " + std::to_string(jrc::Stage::get().get_mapid())
            );
        }
    }
}

namespace jrc
{
    void FieldObstacleOnoffHandler::handle(InPacket& recv) const
    {
        const std::string name = recv.read_string();
        const int32_t state = recv.read_int();

        apply_obstacle_state(name, state);
    }

    void FieldObstacleListHandler::handle(InPacket& recv) const
    {
        const int32_t count = recv.read_int();
        for (int32_t i = 0; i < count; ++i)
        {
            const std::string name = recv.read_string();
            const int32_t state = recv.read_int();

            apply_obstacle_state(name, state);
        }
    }

    void FieldObstacleResetHandler::handle(InPacket&) const
    {
        const size_t reset = Stage::get().reset_named_objects();
        Console::get().print(
            "[FieldObstacleResetHandler] Reset " + std::to_string(reset) +
            " named field object(s)"
        );
    }

    void ScriptProgressMessageHandler::handle(InPacket& recv) const
    {
        const std::string message = recv.read_string();
        if (!message.empty())
        {
            // The banner is not toggled, so re-emplacing replaces it.
            UI::get().emplace<UIProgressBanner>(message);
        }
    }
}
