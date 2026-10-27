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
#pragma once
#include "../../../Graphics/Animation.h"

namespace jrc::meso_explosion
{
    // The coin-burst animation shown where a meso pile detonates. Officially
    // the client plays skill 4211006's "effect" at every exploded pile
    // (REMOVE_ITEM_FROM_MAP mode 4), not on the caster.
    const Animation& explosion();

    // Detonation sound (Sound/Skill.img/4211006/Hit), played once per pile.
    void play_sound();
}
