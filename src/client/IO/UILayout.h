#pragma once
#include "../Constants.h"
#include "../Template/Point.h"

#include <algorithm>

namespace jrc
{
    // Shared positioning for screens authored against the 800x600 canvas.
    namespace ui_layout
    {
        constexpr int16_t DESIGN_WIDTH = 800;
        constexpr int16_t DESIGN_HEIGHT = 600;

        // Top-left corner of the design canvas inside the current viewport.
        inline Point<int16_t> origin()
        {
            const int16_t x = static_cast<int16_t>(std::max(
                0, (Constants::viewwidth() - DESIGN_WIDTH) / 2
            ));
            const int16_t y = static_cast<int16_t>(std::max(
                0, (Constants::viewheight() - DESIGN_HEIGHT) / 2
            ));
            return Point<int16_t>(x, y);
        }

        // Centered box top-left for a box of the given design size.
        inline Point<int16_t> centered_box(int16_t width, int16_t height)
        {
            return origin() + Point<int16_t>(
                static_cast<int16_t>((DESIGN_WIDTH - width) / 2),
                static_cast<int16_t>((DESIGN_HEIGHT - height) / 2)
            );
        }
    }
}
