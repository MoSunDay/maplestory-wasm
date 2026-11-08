// Read-only e2e probes for the world-map / region-map window.
//
// The region navigation (MapLink clicks, parentMap ESC hops) is driven by
// data that only exists at runtime (lazy NX chunks, saved window position),
// so pixel screenshots cannot tell "link missing" from "link off-screen".
// These probes expose the window geometry, the created link buttons and the
// resolved map names (as FNV-1a hashes) for the CDP driver.
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIWorldMap.h"

#include <cstdint>

namespace
{
    jrc::UIWorldMap* worldmap_element()
    {
        jrc::Optional<jrc::UIWorldMap> element =
            jrc::UI::get().get_element<jrc::UIWorldMap>();
        return element ? element.get() : nullptr;
    }
}

extern "C"
{
    int msworldmap_active()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map && map->is_active() ? 1 : 0;
    }

    int msworldmap_geometry(int which)
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? map->debug_geometry(which) : -1;
    }

    int msworldmap_link_count()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? static_cast<int>(map->debug_link_count()) : -1;
    }

    int msworldmap_link_bounds(int index, int corner)
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? map->debug_link_bounds(index, corner) : -1;
    }

    int msworldmap_link_state(int index)
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? map->debug_link_state(index) : -1;
    }

    int msworldmap_resolved_hash()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? static_cast<int>(map->debug_resolved_hash()) : 0;
    }

    int msworldmap_user_hash()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? static_cast<int>(map->debug_user_hash()) : 0;
    }

    int msworldmap_parent_hash()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? static_cast<int>(map->debug_parent_hash()) : 0;
    }

    int msworldmap_link_target_hash(int index)
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? static_cast<int>(map->debug_link_target_hash(index)) : 0;
    }

    int msworldmap_link_hit(int x, int y)
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map ? map->debug_link_hit(x, y) : -1;
    }

    int msworldmap_search()
    {
        const jrc::UIWorldMap* map = worldmap_element();
        return map && map->debug_search() ? 1 : 0;
    }

    int msworldmap_name_hash(const char* name)
    {
        return name
            ? static_cast<int>(jrc::UIWorldMap::debug_name_hash(name))
            : 0;
    }
}
