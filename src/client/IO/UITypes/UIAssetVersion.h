//////////////////////////////////////////////////////////////////////////////
// Shared UI asset generation probes ("post-Chaos" vs legacy v83).
//
// Two probe strategies exist; choose per window, never per convenience:
//
// 1. Global dump marker - ui_assets::post_chaos_global() below. The
//    post-Chaos UI.nx ships Login.img/WorldSelect/BtChannel, the
//    pre-Chaos (v83) UI.nx does not. Use it only for windows whose whole
//    art set flips with the major version (npc talk UtilDlgEx, item
//    inventory Item), where the two generations cannot coexist in one
//    package.
//
// 2. Window-local probe - ask the window's own tree (storage keeps
//    legacy() = UIWindow.img/Trunk/en exists). Use it when both art
//    generations ship side by side inside a modern package: the trunk
//    dump reports the post-Chaos global marker yet still packs the
//    legacy 463x318 list frame, so keying the trunk off the global
//    marker mismatched the art against the layout constants and
//    rendered a 263px fragment (2026-10-25, commit 16b44e2). Never
//    "fix" such a window by swapping its local probe for the global
//    marker.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace ui_assets
    {
        // Global generation marker: post-Chaos UI.nx ships
        // Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos_global()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }
    }
}
