//////////////////////////////////////////////////////////////////////////////
// Generation geometry for the equip inventory window.
//
// The window is shipped in two shapes: post-Chaos splits it into the
// layered UIWindow2.img/Equip/character panel (184 wide) plus the
// UIWindow2.img/Equip/pet art, while pre-Chaos (v83) paints one
// UIWindow.img/Equip bitmap (175 wide) with the pet panel as its "pet"
// sub node.  The socket grids and the pet item/potion hit boxes differ
// per generation; these free functions pick the right values at runtime
// with the WorldSelect marker shared with UiAssetVersion.h so the window
// code stays linear.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Character/Look/EquipSlot.h"
#include "../../Template/EnumMap.h"
#include "../../Template/Point.h"
#include "../../Template/Rectangle.h"

#include "nlnx/nx.hpp"

#include <array>
#include <cstdint>

namespace jrc
{
    namespace equipinv_nodes
    {
        // Generation marker shared with UiAssetVersion.h: post-Chaos UI.nx
        // ships Login.img/WorldSelect/BtChannel, pre-Chaos does not.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        // Default drag anchor: the post-Chaos character panel is 9px wider.
        inline Point<int16_t> window_position()
        {
            return post_chaos()
                ? Point<int16_t>(184, 20)
                : Point<int16_t>(175, 20);
        }

        // Element dimension matching the shipped character bitmap.
        inline Point<int16_t> dimension()
        {
            return post_chaos()
                ? Point<int16_t>(184, 290)
                : Point<int16_t>(175, 304);
        }

        using IconGrid = EnumMap<Equipslot::Id, Point<int16_t>>;

        // Socket grid of the loaded generation (32px cells).  Unmapped
        // slots keep (0, 0), exactly like the previous inline tables.
        inline IconGrid icon_positions()
        {
            IconGrid grid;
            if (post_chaos())
            {
                grid[Equipslot::CAP]      = Point<int16_t>( 43,  25);
                grid[Equipslot::FACEACC]  = Point<int16_t>( 43,  91);
                grid[Equipslot::EYEACC]   = Point<int16_t>( 43,  68);
                grid[Equipslot::EARRINGS] = Point<int16_t>(109,  91);
                grid[Equipslot::TOP]      = Point<int16_t>( 43, 124);
                grid[Equipslot::PANTS]    = Point<int16_t>( 43, 157);
                grid[Equipslot::SHOES]    = Point<int16_t>( 76, 190);
                grid[Equipslot::GLOVES]   = Point<int16_t>( 10, 157);
                grid[Equipslot::CAPE]     = Point<int16_t>( 10, 124);
                grid[Equipslot::SHIELD]   = Point<int16_t>(142, 124);
                grid[Equipslot::WEAPON]   = Point<int16_t>(109, 124);
                grid[Equipslot::RING]     = Point<int16_t>(109, 157);
                grid[Equipslot::RING2]    = Point<int16_t>(142, 157);
                grid[Equipslot::RING3]    = Point<int16_t>(109,  91);
                grid[Equipslot::RING4]    = Point<int16_t>(142,  91);
                grid[Equipslot::PENDANT]  = Point<int16_t>( 76, 124);
                grid[Equipslot::TAMEDMOB] = Point<int16_t>(142,  91);
                grid[Equipslot::SADDLE]   = Point<int16_t>( 76, 124);
                grid[Equipslot::MEDAL]    = Point<int16_t>( 10,  58);
                grid[Equipslot::BELT]     = Point<int16_t>( 76, 157);
            }
            else
            {
                // v83 keeps the whole character panel in one bitmap.  The
                // labelled sockets form the same five-column grid the
                // later files use (32px cells, 33px pitch) shifted by
                // (-5, +10); only cells that carry a socket in the bitmap
                // are mapped.
                grid[Equipslot::CAP]      = Point<int16_t>( 38,  35);
                grid[Equipslot::MEDAL]    = Point<int16_t>(  5,  68);
                grid[Equipslot::EYEACC]   = Point<int16_t>( 38,  68);
                grid[Equipslot::RING3]    = Point<int16_t>(104,  68);
                grid[Equipslot::RING4]    = Point<int16_t>(137,  68);
                grid[Equipslot::FACEACC]  = Point<int16_t>( 71, 101);
                grid[Equipslot::EARRINGS] = Point<int16_t>(104, 101);
                grid[Equipslot::CAPE]     = Point<int16_t>(  5, 134);
                grid[Equipslot::TOP]      = Point<int16_t>( 38, 134);
                grid[Equipslot::PENDANT]  = Point<int16_t>( 71, 134);
                grid[Equipslot::WEAPON]   = Point<int16_t>(104, 134);
                grid[Equipslot::SHIELD]   = Point<int16_t>(137, 134);
                grid[Equipslot::GLOVES]   = Point<int16_t>(  5, 167);
                grid[Equipslot::PANTS]    = Point<int16_t>( 38, 167);
                grid[Equipslot::BELT]     = Point<int16_t>( 71, 167);
                grid[Equipslot::RING]     = Point<int16_t>(104, 167);
                grid[Equipslot::RING2]    = Point<int16_t>(137, 167);
                grid[Equipslot::SHOES]    = Point<int16_t>( 71, 200);
                grid[Equipslot::TAMEDMOB] = Point<int16_t>(  5, 233);
                grid[Equipslot::SADDLE]   = Point<int16_t>( 38, 233);
            }
            return grid;
        }

        // Pet panel anchor, relative to the window position.
        inline Point<int16_t> pet_panel_origin()
        {
            return post_chaos()
                ? Point<int16_t>(184, 0)
                : Point<int16_t>(175, 0);
        }

        // Auto potion sockets on the pet panel.
        inline Point<int16_t> pet_hp_pos()
        {
            return post_chaos() ? Point<int16_t>(194, 43) : Point<int16_t>(188, 44);
        }

        inline Point<int16_t> pet_mp_pos()
        {
            return post_chaos() ? Point<int16_t>(234, 43) : Point<int16_t>(221, 44);
        }

        // Sockets of the five pet ability items.
        inline std::array<Point<int16_t>, 5> pet_ability_positions()
        {
            if (post_chaos())
            {
                return {
                    Point<int16_t>(194, 88), Point<int16_t>(230, 88),
                    Point<int16_t>(266, 88), Point<int16_t>(212, 124),
                    Point<int16_t>(248, 124)
                };
            }
            return {
                Point<int16_t>(188, 77), Point<int16_t>(221, 77),
                Point<int16_t>(254, 77), Point<int16_t>(204, 110),
                Point<int16_t>(237, 110)
            };
        }

        // Full pet panel bounds while it is open, in window coordinates.
        inline Rectangle<int16_t> pet_panel_bounds(Point<int16_t> position)
        {
            return post_chaos()
                ? Rectangle<int16_t>(
                    position + Point<int16_t>(184, 0),
                    position + Point<int16_t>(304, 185))
                : Rectangle<int16_t>(
                    position + Point<int16_t>(175, 0),
                    position + Point<int16_t>(352, 181));
        }

        // Auto potion drop hit boxes (32px cells), in window coordinates.
        inline Rectangle<int16_t> pet_hp_bounds(Point<int16_t> position)
        {
            return post_chaos()
                ? Rectangle<int16_t>(
                    position + Point<int16_t>(194, 43),
                    position + Point<int16_t>(226, 75))
                : Rectangle<int16_t>(
                    position + Point<int16_t>(188, 44),
                    position + Point<int16_t>(220, 76));
        }

        inline Rectangle<int16_t> pet_mp_bounds(Point<int16_t> position)
        {
            return post_chaos()
                ? Rectangle<int16_t>(
                    position + Point<int16_t>(234, 43),
                    position + Point<int16_t>(266, 75))
                : Rectangle<int16_t>(
                    position + Point<int16_t>(221, 44),
                    position + Point<int16_t>(253, 76));
        }
    }
}
