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
#include "SummonAnims.h"

#include <cstring>

#include "../../Util/Misc.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <unordered_map>

namespace jrc
{
    namespace summon
    {
        namespace
        {
            // Animation state nodes under a skill's 'summon' node. Flying
            // summons (birds, dragons, gulls) animate through 'fly' and
            // ground summons (octopus, beholder) through 'move', so the
            // resolver picks whichever the skill ships.
            nl::node summon_source(int32_t skillid)
            {
                const std::string strid =
                    string_format::extend_id(skillid, 7);
                return nl::nx::skill[string_format::skill_image(skillid)]
                    ["skill"][strid]["summon"];
            }

            Animations build_animations(nl::node src)
            {
                Animations anims;
                if (!src)
                    return anims;

                anims.summoned = src["summoned"];
                anims.stand = src["stand"];
                anims.attack = src["attack1"];
                anims.die = src["die"];

                nl::node fly = src["fly"];
                nl::node move = src["move"];
                if (fly)
                {
                    anims.move = fly;
                    anims.has_move = true;
                }
                else if (move)
                {
                    anims.move = move;
                    anims.has_move = true;
                }

                anims.valid = anims.stand.is_valid()
                    || anims.move.is_valid()
                    || anims.summoned.is_valid();
                return anims;
            }
        }

        const Animations& summon_animation(int32_t skillid)
        {
            // Static cache of resolved skill sets; a miss resolves once and
            // stays invalid, so missing data degrades to an invisible but
            // fully synced entity instead of an error.
            static std::unordered_map<int32_t, Animations> cache;
            static const Animations empty;

            auto iter = cache.find(skillid);
            if (iter != cache.end())
                return iter->second;

            return cache.emplace(
                skillid, build_animations(summon_source(skillid))
            ).first->second;
        }

        int32_t dragon_skillid(int16_t job)
        {
            switch (job)
            {
                // Priest 'Summon Dragon'. The shipped Skill.nx carries no
                // node for it, so it renders invisible by design.
                case 231:
                    return 2311006;
                // Bishop 'Bahamut'.
                case 232:
                    return 2321003;
                default:
                    return 0;
            }
        }
    }
}
