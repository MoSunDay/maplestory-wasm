//////////////////////////////////////////////////////////////////////////////
// Flat 2023-style chrome for the character select screen.
//
// The panels are drawn procedurally (CashShop-style flat rectangles)
// between the backdrop sprites and the NX accent art, so the screen keeps
// working even when optional accent nodes are missing from UI.nx. All
// functions are stateless free functions taking the design canvas origin.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../Components/Button.h"
#include "../../Template/Point.h"

#include <cstdint>

namespace jrc
{
    namespace charselect_panels
    {
        // Static chrome: top bar, left stage band and the four-row info
        // card with its captions. Draw after the backdrop sprites and
        // before buttons or roster art.
        void draw_chrome(Point<int16_t> origin);

        // Backing pill behind the NX START button; the tint follows the
        // button state so hover/pressed/disabled stay readable.
        void draw_start_backing(
            Point<int16_t> origin,
            Button::State state
        );

        // Backing pills behind the create/delete buttons.
        void draw_footer_backing(
            Point<int16_t> origin,
            Button::State create_state,
            Button::State delete_state
        );

        // Hover and selection highlight around roster slot i. The orange
        // selection outline marks the chosen character, the translucent
        // white fill brightens the hovered card.
        void draw_slot_highlight(
            Point<int16_t> origin,
            uint8_t slot,
            bool selected,
            Button::State state
        );
    }
}
