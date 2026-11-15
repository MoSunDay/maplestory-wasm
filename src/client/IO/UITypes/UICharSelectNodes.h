//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the modern character select screen.
//
// The 2023 chrome is drawn procedurally, so UI.nx only supplies accents:
// the roster platform, the purchase art for empty slots and the Lv
// charset. Every lookup is null safe: a node missing from the loaded
// UI.nx resolves to an empty sprite that simply draws nothing, and the
// screen logs a Console warning at init time instead of failing.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace charselect_nodes
    {
        inline nl::node root()
        {
            return nl::nx::ui["Login.img"]["CharSelect"];
        }

        // character/0 is the disc platform animation under each roster
        // slot (62x8 frames, origin (29,7)).
        inline nl::node platform()
        {
            return root()["character"]["0"];
        }

        // buyCharacter is the purchase card art filling unused slots.
        inline nl::node empty_slot()
        {
            return root()["buyCharacter"];
        }

        // The dedicated Lv charset on the info card header.
        inline bool has_level_charset()
        {
            return static_cast<bool>(root()["lv"]);
        }
    }
}
