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
#include "ResolutionStore.h"

#ifdef __EMSCRIPTEN__
#include "Resolution.h"

#include <emscripten.h>
#endif

namespace jrc
{
    namespace display_store
    {
#ifdef __EMSCRIPTEN__
        namespace
        {
            // Single source of truth for the persisted preset index.
            constexpr const char* STORAGE_KEY = "maplestory.display.resolution";
        }
#endif

        std::size_t load_index()
        {
#ifdef __EMSCRIPTEN__
            const int parsed = EM_ASM_INT({
                try {
                    const stored = window.localStorage.getItem(UTF8ToString($0));
                    if (stored === null) return -1;

                    const value = Number.parseInt(stored, 10);
                    if (!Number.isFinite(value)) return -1;
                    if (String(value) !== stored.trim()) return -1;
                    if (value < 0 || value >= $1) return -1;
                    return value;
                } catch (_) {
                    return -1;
                }
            }, STORAGE_KEY, static_cast<int>(display::preset_count()));
            return display::normalize_index(parsed);
#else
            return 0;
#endif
        }

        bool save_index(std::size_t index)
        {
#ifdef __EMSCRIPTEN__
            const int normalized = static_cast<int>(
                display::normalize_index(static_cast<int>(index))
            );
            return EM_ASM_INT({
                try {
                    window.localStorage.setItem(
                        UTF8ToString($0),
                        String($1)
                    );
                    return 1;
                } catch (_) {
                    return 0;
                }
            }, STORAGE_KEY, normalized) != 0;
#else
            (void)index;
            return true;
#endif
        }
    }
}
