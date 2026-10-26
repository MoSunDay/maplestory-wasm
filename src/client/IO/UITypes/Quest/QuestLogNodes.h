//////////////////////////////////////////////////////////////////////////////
// Node resolution helpers for the quest log window.
//
// The pre-Chaos (v83) UI.nx ships the whole quest window under
// UIWindow.img/Quest: backgrnd (245x396) is the left list pane and
// backgrnd3 (296x289) the right info pane. The post-Chaos UI.nx moves the
// same furniture into UIWindow2.img/Quest/list and
// UIWindow2.img/Quest/quest_info, so only the roots differ between the
// generations and the info pane is always looked up separately.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIAssetVersion.h"

#include "nlnx/nx.hpp"

#include <cstdint>

namespace jrc
{
    namespace questlog_nodes
    {
        // Global generation marker; probe lives in UIAssetVersion.h.
        inline bool post_chaos()
        {
            return ui_assets::post_chaos_global();
        }

        // Left pane: the quest list with its tabs.
        inline nl::node list_src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Quest"]["list"]
                : nl::nx::ui["UIWindow.img"]["Quest"];
        }

        // Right pane: the selected quest's summary.
        inline nl::node info_src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Quest"]["quest_info"]
                : nl::nx::ui["UIWindow.img"]["Quest"];
        }

        // The tab row always holds disabled/enabled sprite lists indexed
        // 0..3; the log window only uses the first two tabs.
        inline nl::node tab_disabled(uint8_t index)
        {
            return list_src()["Tab"]["disabled"][index];
        }

        inline nl::node tab_enabled(uint8_t index)
        {
            return list_src()["Tab"]["enabled"][index];
        }

        // 'Completed N/M' counter strip; only the post-Chaos list ships it.
        inline nl::node complete_count_src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["Quest"]["list"]["completeCount"]
                : nl::node();
        }
    }
}
