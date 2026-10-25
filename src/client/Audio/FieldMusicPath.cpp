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
#include "FieldMusicPath.h"

namespace jrc::field_music
{
    std::string normalize_bgm_path(const std::string& path)
    {
        if (path.empty())
        {
            return {};
        }

        if (path.find(".img") != std::string::npos)
        {
            return path;
        }

        const size_t split = path.find('/');
        if (split == std::string::npos)
        {
            return path + ".img";
        }

        return path.substr(0, split) + ".img" + path.substr(split);
    }
}
