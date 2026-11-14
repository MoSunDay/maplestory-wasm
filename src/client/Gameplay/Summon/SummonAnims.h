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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
// GNU Affero General Public License for more details.                     //
#pragma once
#include "../../Graphics/Animation.h"

#include <cstdint>

namespace jrc
{
    /// Remote summons (archer puppets, birds, priest dragons, pirate
    /// octopi, dark knight beholders, ...) synced through the summon
    /// packets. Plain data plus free functions only.
    namespace summon
    {
        /// One skill's summon animation set. Summon frames live under
        /// `Skill/<book>.img/skill/<id>/summon/<state>` where <state> is
        /// one of 'summoned', 'stand', 'move' or 'fly', 'attack1' and
        /// 'die' (probed against the shipped Skill.nx). Flying summons
        /// use 'fly', ground summons 'move'; the resolver accepts either.
        struct Animations
        {
            Animation summoned;
            Animation stand;
            Animation move;
            Animation attack;
            Animation die;

            bool has_move = false;
            bool valid = false;
        };

        /// Resolve the animation set of a summon skill. Results are
        /// cached per skill id; skills without a 'summon' node return an
        /// invalid set, which keeps the entity invisible while its
        /// position and lifetime stay synced.
        const Animations& summon_animation(int32_t skillid);

        /// Skill id a SPAWN_DRAGON packet renders with for the owner's
        /// job (priest dragon, bishop bahamut, ...). Returns 0, meaning
        /// "no animation", for unmapped jobs.
        int32_t dragon_skillid(int16_t job);
    }
}
