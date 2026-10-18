//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the key configuration window.
//
// The upstream key config reads UIWindow2.img/KeyConfig, which only exists
// in the post-Chaos UI.nx. The pre-Chaos (v83) asset set has no UIWindow2
// and ships the same window as a single bitmap under UIWindow.img/KeyConfig
// (backgrnd 629x373 with its own BtClose, instead of the layered
// backgrnd/backgrnd2/backgrnd3 art and the shared Basic.img close button).
// The generation is probed at runtime with the WorldSelect marker shared
// with UiAssetVersion.h; every lookup is null safe.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace keyconfig_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        // The KeyConfig subtree of the loaded asset set: UIWindow2.img for
        // post-Chaos, UIWindow.img for pre-Chaos (v83).
        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["KeyConfig"]
                : nl::nx::ui["UIWindow.img"]["KeyConfig"];
        }
    }
}
