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
// Server-driven skill cooldowns: store seconds as milliseconds and tick
// them down once per fixed update step.
#include "Player.h"

#include "../Constants.h"

namespace jrc
{
    void Player::add_cooldown(int32_t skill_id, int32_t seconds)
    {
        if (skill_id <= 0 || seconds <= 0)
        {
            cooldowns.erase(skill_id);
            return;
        }

        cooldowns[skill_id] = seconds * 1000;
    }

    bool Player::has_cooldown(int32_t skill_id) const
    {
        auto iter = cooldowns.find(skill_id);
        return iter != cooldowns.end() && iter->second > 0;
    }

    int32_t Player::get_cooldown_seconds(int32_t skill_id) const
    {
        auto iter = cooldowns.find(skill_id);
        if (iter == cooldowns.end() || iter->second <= 0)
        {
            return 0;
        }

        return (iter->second + 999) / 1000;
    }

    void Player::update_cooldowns()
    {
        for (auto iter = cooldowns.begin(); iter != cooldowns.end();)
        {
            if (iter->second <= Constants::TIMESTEP)
            {
                iter = cooldowns.erase(iter);
            }
            else
            {
                iter->second -= Constants::TIMESTEP;
                ++iter;
            }
        }
    }
}
