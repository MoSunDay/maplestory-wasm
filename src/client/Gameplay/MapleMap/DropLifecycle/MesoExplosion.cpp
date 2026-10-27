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
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "MesoExplosion.h"

#include "../../../Audio/Audio.h"

#include "nlnx/nx.hpp"

namespace jrc::meso_explosion
{
    namespace
    {
        // Meso Explosion (4211006) skill nodes. Loaded lazily so maps
        // without a bandit never touch them.
        nl::node effect_node()
        {
            return nl::nx::skill["421.img"]["skill"]["4211006"]["effect"];
        }

        nl::node sound_node()
        {
            return nl::nx::sound["Skill.img"]["4211006"]["Hit"];
        }
    }

    const Animation& explosion()
    {
        static const Animation animation(effect_node());
        return animation;
    }

    void play_sound()
    {
        static const Sound sound(sound_node());
        sound.play();
    }
}
