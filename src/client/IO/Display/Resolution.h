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

#include <cstddef>
#include <cstdint>
#include <string>

namespace jrc
{
    namespace display
    {
        // A supported client render resolution.
        struct Resolution
        {
            int16_t width;
            int16_t height;
        };

        // Number of supported presets. Index 0 is the default.
        std::size_t preset_count();

        // Preset at index; out-of-range indices fall back to the default.
        Resolution preset(std::size_t index);

        // The default preset (800 x 600).
        Resolution default_resolution();

        // Clamp an arbitrary index to a valid preset index.
        std::size_t normalize_index(int index);

        // Cycle through presets. Positive direction moves forward,
        // negative moves backward; both directions wrap around.
        std::size_t next_index(std::size_t current, int direction);

        // Preset index matching an exact width/height pair.
        // Returns 0 for anything that is not a supported preset.
        std::size_t index_of(int16_t width, int16_t height);

        // Whether the resolution is one of the supported presets.
        bool valid(Resolution resolution);

        // Human-readable label for a preset, e.g. "800 x 600".
        // Unsupported resolutions fall back to the default label.
        std::string label(Resolution resolution);
    }
}
