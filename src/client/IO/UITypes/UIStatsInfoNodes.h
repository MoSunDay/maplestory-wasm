//////////////////////////////////////////////////////////////////////////////
// Generation helpers for the character stat window.
//
// The stat window lives in different subtrees per UI.nx generation:
// post-Chaos splits it into UIWindow4.img/Stat/main plus a detachable
// UIWindow4.img/Stat/detail panel (212 wide, 15 detail rows, ability
// titles), while pre-Chaos (v83) ships one UIWindow.img/Stat node with
// the 175x347 main panel and the backgrnd2 detail bitmap.  The window
// geometry is chosen at runtime with the WorldSelect marker shared with
// UiAssetVersion.h; the row layout itself lives in the window code.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

#include <cstdint>

namespace jrc
{
    namespace statsinfo_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        // Default drag anchor: the post-Chaos main panel is 37px wider.
        inline Point<int16_t> window_position()
        {
            return post_chaos()
                ? Point<int16_t>(212, 20)
                : Point<int16_t>(175, 20);
        }
    }
}
