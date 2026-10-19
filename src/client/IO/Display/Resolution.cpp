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
#include "Resolution.h"

namespace jrc
{
    namespace display
    {
        namespace
        {
            constexpr Resolution PRESETS[] = {
                {800, 600},
                {1280, 720}
            };
            constexpr std::size_t PRESET_COUNT =
                sizeof(PRESETS) / sizeof(PRESETS[0]);
        }

        std::size_t preset_count()
        {
            return PRESET_COUNT;
        }

        Resolution preset(std::size_t index)
        {
            return PRESETS[index < PRESET_COUNT ? index : 0];
        }

        Resolution default_resolution()
        {
            return PRESETS[0];
        }

        std::size_t normalize_index(int index)
        {
            return index >= 0 && static_cast<std::size_t>(index) < PRESET_COUNT
                ? static_cast<std::size_t>(index)
                : 0;
        }

        std::size_t next_index(std::size_t current, int direction)
        {
            const long long count = static_cast<long long>(PRESET_COUNT);
            const long long base = static_cast<long long>(
                current < PRESET_COUNT ? current : 0
            );
            long long next = (base + static_cast<long long>(direction)) % count;
            if (next < 0)
            {
                next += count;
            }
            return static_cast<std::size_t>(next);
        }

        std::size_t index_of(int16_t width, int16_t height)
        {
            for (std::size_t i = 0; i < PRESET_COUNT; ++i)
            {
                if (PRESETS[i].width == width && PRESETS[i].height == height)
                {
                    return i;
                }
            }
            return 0;
        }

        bool valid(Resolution resolution)
        {
            for (std::size_t i = 0; i < PRESET_COUNT; ++i)
            {
                if (PRESETS[i].width == resolution.width &&
                    PRESETS[i].height == resolution.height)
                {
                    return true;
                }
            }
            return false;
        }

        std::string label(Resolution resolution)
        {
            const Resolution supported = preset(
                index_of(resolution.width, resolution.height)
            );
            return std::to_string(supported.width) + " x " +
                std::to_string(supported.height);
        }
    }
}
