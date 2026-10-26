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

#include "UIAssetVersion.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace npctalk_nodes
    {
        // Global generation marker; probe lives in UIAssetVersion.h.
        inline bool post_chaos()
        {
            return ui_assets::post_chaos_global();
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["UtilDlgEx"]
                : nl::nx::ui["UIWindow.img"]["UtilDlgEx"];
        }
    }
}
