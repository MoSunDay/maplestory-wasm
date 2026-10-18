//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the character select screen.
//
// pre-Chaos (v83) UI.nx keeps the character select under
// Login.img/CharSelect but ships a different node set than the post-Chaos
// assets the upstream code was written against: there is no charSlot
// window, no buyCharacter art, no selectedWorld banner and no lv charset.
// These free functions pick the right source node per generation so the
// screen code can stay linear. Every lookup is null safe: a node that is
// missing in the loaded UI.nx resolves to an empty sprite/label that
// simply draws nothing.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Text.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace charselect_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        inline nl::node root()
        {
            return nl::nx::ui["Login.img"]["CharSelect"];
        }

        // Post-Chaos draws the selected character data on charInfo;
        // pre-Chaos ships the taller charInfo1 variant whose extra rows
        // leave room for the 2x2 stat table (both 183 wide, origin (45,57)).
        inline nl::node info_board()
        {
            const nl::node src = root();
            return post_chaos() ? src["charInfo"] : src["charInfo1"];
        }

        // Post-Chaos: character/0 is the slot platform. Pre-Chaos: the
        // platform is character/1 (51x71, origin (25,72)) while
        // character/0 is only a small shine animation, so keying on node
        // presence alone would pick the wrong bitmap.
        inline nl::node platform()
        {
            const nl::node src = root()["character"];
            return post_chaos() ? src["0"] : src["1"];
        }

        // Post-Chaos fills unused slots with the buyCharacter purchase
        // art; pre-Chaos marks them with the small icon/new badge instead.
        inline nl::node empty_slot()
        {
            const nl::node src = root();
            const nl::node buyslot = src["buyCharacter"];
            return buyslot ? buyslot : src["icon"]["new"];
        }

        // pageL/pageR only carry normal/pressed state subtrees post-Chaos;
        // pre-Chaos stores plain frame pairs (0 = normal, 1 = pressed).
        inline bool pages_are_state_buttons()
        {
            return post_chaos() || static_cast<bool>(root()["pageL"]["normal"]);
        }

        // The dedicated Lv charset only exists post-Chaos.
        inline bool has_level_charset()
        {
            return static_cast<bool>(root()["lv"]);
        }

        // Pre-Chaos fallback for the Lv marker: a plain label. The label
        // is a function local static because Text caches its layout and
        // change_text() is a no-op while the level stays the same, so the
        // per frame cost is a string compare.
        inline Text& level_label()
        {
            static Text label{ Text::A11M, Text::CENTER, Text::WHITE };
            return label;
        }
    }
}
