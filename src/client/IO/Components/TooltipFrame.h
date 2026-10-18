//////////////////////////////////////////////////////////////////////////////
// Self-drawn tooltip frame fallback.
//
// The upstream tooltips read their chrome from UIToolTip.img (Frame,
// Frame2, cover) and UIWindow2.img/ToolTip/WorldMap. The pre-Chaos (v83)
// UI.nx ships neither file: UIToolTip.img is absent entirely and
// UIWindow.img/ToolTip only holds the Equip label strips. The frame art
// therefore cannot be resolved from nx and is drawn with plain color
// quads instead: a dark translucent fill with a light 1px bevel, anchored
// like MapleFrame (position = bottom center of the box).
//
// Free functions only; no state is kept.
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../Graphics/Geometry.h"
#include "../../Template/Point.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace tooltip_frame
    {
        // True when the post-Chaos frame art is loadable. Tooltips keep
        // using the nx chrome when it exists.
        inline bool has_assets()
        {
            return static_cast<bool>(nl::nx::ui["UIToolTip.img"]["Item"]["Frame2"]["c"]);
        }

        // Draws the fallback chrome for a width x height box whose top left
        // corner sits at topleft.
        inline void draw_at(Point<int16_t> topleft, int16_t width, int16_t height)
        {
            if (width < 3 || height < 3)
            {
                return;
            }

            // Translucent backdrop.
            ColorBox(width - 2, height - 2, Geometry::BLACK, 0.80f)
                .draw(DrawArgument(topleft + Point<int16_t>(1, 1)));

            // Light 1px bevel so the box reads against any background.
            const ColorBox edge(1, 1, Geometry::WHITE, 0.35f);
            edge.draw(DrawArgument(topleft + Point<int16_t>(0, 0), Point<int16_t>(width, 0)));
            edge.draw(DrawArgument(topleft + Point<int16_t>(0, height - 1), Point<int16_t>(width, 0)));
            edge.draw(DrawArgument(topleft + Point<int16_t>(0, 0), Point<int16_t>(0, height)));
            edge.draw(DrawArgument(topleft + Point<int16_t>(width - 1, 0), Point<int16_t>(0, height)));
        }

        // MapleFrame-compatible anchor: position marks the bottom center of
        // the box.
        inline void draw(Point<int16_t> position, int16_t width, int16_t height)
        {
            draw_at(
                Point<int16_t>(position.x() - width / 2, position.y() - height),
                width,
                height
            );
        }
    }
}
