//////////////////////////////////////////////////////////////////////////////
// Node resolution and layout helpers for the world map window.
//
// Post-Chaos UI.nx keeps a single ready-made WorldMap frame sprite
// (Border/0) plus a search panel (WorldMapSearch, BtSearch, BtAutoFly_1,
// BtNaviRegister). The pre-Chaos (v83) UI.nx only ships nine-slice border
// pieces (Border/0..7) and a small title bitmap; there is no search UI at
// all. The actual map contents (BaseImg, MapList, MapLink, info) come
// from Map.nx/WorldMap in both generations and are read by
// UIWorldMap::update_world, which needs no migration.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Texture.h"
#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace worldmap_nodes
    {
        // Generation marker shared with UiAssetVersion.h.
        inline bool post_chaos()
        {
            const nl::node marker = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
            return static_cast<bool>(marker);
        }

        inline nl::node src()
        {
            return post_chaos()
                ? nl::nx::ui["UIWindow2.img"]["WorldMap"]
                : nl::nx::ui["UIWindow.img"]["WorldMap"];
        }

        // The world-map images come from Map.nx in both generations.
        inline nl::node worldmap_data()
        {
            return nl::nx::map["WorldMap"];
        }

        // Search panel assets only exist post-Chaos.
        inline bool has_search()
        {
            return post_chaos();
        }

        inline Point<int16_t> base_image_size()
        {
            const Texture base{ worldmap_data()["WorldMap.img"]["BaseImg"]["0"] };
            return base.get_dimensions();
        }

        // Nine-slice metrics of the v83 frame: 7px side borders, 33px top
        // band, 18px bottom band.
        inline constexpr int16_t BORDER_W = 7;
        inline constexpr int16_t TOP_H = 33;
        inline constexpr int16_t BOTTOM_H = 18;

        // Top-left of the map image inside the window.
        inline Point<int16_t> content_pos()
        {
            return { BORDER_W, TOP_H };
        }

        // Outer window size.
        inline Point<int16_t> frame_dimensions()
        {
            const Point<int16_t> base_size = base_image_size();
            return {
                static_cast<int16_t>(base_size.x() + 2 * BORDER_W),
                static_cast<int16_t>(base_size.y() + TOP_H + BOTTOM_H)
            };
        }

        // Draw position of the BaseImg. Post-Chaos images carry a centered
        // origin, so they are drawn at the window-relative spot base.
        inline Point<int16_t> image_draw_pos(Point<int16_t> spot_base)
        {
            return post_chaos() ? spot_base : content_pos();
        }

        // Window-relative center of the map image; markers, links and paths
        // are placed relative to it.
        inline Point<int16_t> spot_base()
        {
            if (post_chaos())
            {
                return {};
            }

            const Point<int16_t> base_size = base_image_size();
            return {
                static_cast<int16_t>(content_pos().x() + base_size.x() / 2),
                static_cast<int16_t>(content_pos().y() + base_size.y() / 2)
            };
        }

        // Assemble the v83 frame from the Border pieces. The pieces are
        // cached in function-local statics because Texture caches the
        // resolved bitmap dimensions on construction.
        inline void draw_frame(Point<int16_t> position)
        {
            if (post_chaos())
            {
                static const Texture frame{ src()["Border"]["0"] };
                frame.draw(position);
                return;
            }

            static const Texture top_left{ src()["Border"]["0"] };
            static const Texture top{ src()["Border"]["1"] };
            static const Texture top_right{ src()["Border"]["2"] };
            static const Texture left{ src()["Border"]["3"] };
            static const Texture right{ src()["Border"]["4"] };
            static const Texture bottom_left{ src()["Border"]["5"] };
            static const Texture bottom{ src()["Border"]["6"] };
            static const Texture bottom_right{ src()["Border"]["7"] };
            static const Texture title{ src()["title"] };

            const Point<int16_t> size = frame_dimensions();
            const int16_t inner_w = static_cast<int16_t>(size.x() - 2 * BORDER_W);
            const int16_t side_h = static_cast<int16_t>(size.y() - TOP_H - BOTTOM_H);

            top_left.draw(position);
            top.draw({ position + Point<int16_t>(BORDER_W, 0), Point<int16_t>(inner_w, 0) });
            top_right.draw(position + Point<int16_t>(size.x() - BORDER_W, 0));

            left.draw({ position + Point<int16_t>(0, TOP_H), Point<int16_t>(0, side_h) });
            right.draw({
                position + Point<int16_t>(size.x() - BORDER_W, TOP_H),
                Point<int16_t>(0, side_h)
            });

            const Point<int16_t> bottom_pos(0, static_cast<int16_t>(size.y() - BOTTOM_H));
            bottom_left.draw(position + bottom_pos);
            bottom.draw({ position + bottom_pos + Point<int16_t>(BORDER_W, 0), Point<int16_t>(inner_w, 0) });
            bottom_right.draw(position + bottom_pos + Point<int16_t>(size.x() - BORDER_W, 0));

            title.draw(position + Point<int16_t>(
                static_cast<int16_t>((size.x() - title.width()) / 2),
                14
            ));
        }
    }
}
