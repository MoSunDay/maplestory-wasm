//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the npc talk window.
//
// The upstream code reads the UtilDlgEx frame from the post-Chaos
// UIWindow2.img. The pre-Chaos (v83) UI.nx has no UIWindow2.img at all but
// ships the same UtilDlgEx subtree (t/c/s tiles, bar name plate and the
// BtOK/BtNext/BtPrev/BtClose/BtYes/BtNo buttons) directly under
// UIWindow.img, so only the root differs between generations.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace npctalk_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["UtilDlgEx"]
                : nl::nx::ui["UIWindow.img"]["UtilDlgEx"];
        }
    }
}
