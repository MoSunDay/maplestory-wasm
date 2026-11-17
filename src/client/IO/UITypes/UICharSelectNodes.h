//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the character select screen.
//
// The 079MAX4 bitmap screen lives under separately named nodes so the
// login/world-select art stays untouched:
//   Login.img/CharSelect079   complete 079 CharSelect subtree
//   Login.img/frame079        800x600 login window frame
//   Map.nx Back/login079      38 parallax backdrop layers
//
// When those nodes are absent, the helpers fall back to the 2023 flat
// UI.nx tree used before the bitmap port. Every lookup is null safe; a
// missing sprite simply draws nothing and the screen logs a warning.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "nlnx/nx.hpp"

#include <string>

namespace jrc
{
    namespace charselect_nodes
    {
        inline nl::node root()
        {
            return nl::nx::ui["Login.img"]["CharSelect"];
        }

        inline nl::node root079()
        {
            return nl::nx::ui["Login.img"]["CharSelect079"];
        }

        inline nl::node frame079()
        {
            return nl::nx::ui["Login.img"]["frame079"];
        }

        // The injected 150x57 world banner board, same shape as
        // Common/selectWorld; absent from UI.nx files without the port.
        inline nl::node select_world079()
        {
            return nl::nx::ui["Login.img"]["selectWorld079"];
        }

        inline nl::node back079()
        {
            return nl::nx::map["Back"]["login079"];
        }

        // The bitmap screen is selected only when both the 079 char-select
        // subtree and its backdrop exist; otherwise keep the flat fallback.
        inline bool has_bitmap_ui()
        {
            return static_cast<bool>(root079()["charInfo1"]) &&
                   static_cast<bool>(back079()["back"]);
        }

        inline nl::node active_root()
        {
            nl::node legacy = root079();
            return legacy ? legacy : root();
        }

        // character/0 is the disc platform animation under each roster slot.
        inline nl::node platform()
        {
            return active_root()["character"]["0"];
        }

        // 079 uses character/1 as the empty avatar silhouette; the 2023
        // tree uses the buyCharacter purchase card instead.
        inline nl::node empty_slot()
        {
            nl::node legacy = active_root()["character"]["1"];
            return legacy ? legacy : active_root()["buyCharacter"];
        }

        // charInfo1 is the tall card with rank rows; charInfo is the short
        // card used by the 079 fallback.
        inline nl::node info_board()
        {
            nl::node board = active_root()["charInfo1"];
            return board ? board : active_root()["charInfo"];
        }

        inline bool has_long_info()
        {
            return static_cast<bool>(active_root()["charInfo1"]);
        }

        inline nl::node name_tag()
        {
            return active_root()["nameTag"];
        }

        // The dedicated Lv charset on the 2023 info card.
        inline bool has_level_charset()
        {
            return static_cast<bool>(root()["lv"]);
        }

        // 079 job names are 10px-tall bitmaps under icon/job/0..4.
        inline nl::node job_icon(size_t index)
        {
            return root079()["icon"]["job"][std::to_string(index)];
        }

        // Page arrows: 079 wraps each state canvas in a numbered frame
        // (pageL/<state>/<canvas>), while later trees keep the bitmap on the
        // state node itself. Texture only accepts bitmap nodes, so unwrap one
        // level when needed instead of handing it the container.
        inline nl::node page_arrow(bool right, size_t state)
        {
            nl::node frame =
                active_root()[right ? "pageR" : "pageL"][std::to_string(state)];
            if (frame.data_type() == nl::node::type::bitmap)
            {
                return frame;
            }

            nl::node canvas = frame["0"];
            return canvas.data_type() == nl::node::type::bitmap
                ? canvas
                : nl::node{};
        }
    }
}
