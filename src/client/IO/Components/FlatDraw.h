//////////////////////////////////////////////////////////////////////////////
// Flat drawing primitives for procedurally drawn UI panels.
//
// The modern (2023-style) login screens draw their chrome as flat
// rectangles through GraphicsGL instead of NX bitmaps. These free
// functions keep that plumbing in one place: callers pass the design
// canvas origin of their element, positions and sizes stay in 800x600
// design coordinates. Those flow through the shared quad shader like
// every NX sprite, so with VIEWYOFFSET=10 a position (x, y) lands on
// canvas row y+10 - pixel tests and e2e anchors must bake that shift
// in. Everything here is stateless; colour stays with the caller.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/GraphicsGL.h"
#include "../../Template/Point.h"

namespace jrc
{
    namespace flatdraw
    {
        // Opaque filled rectangle in design coordinates.
        inline void fill_rect(Point<int16_t> base, Point<int16_t> position,
            Point<int16_t> size, float r, float g, float b, float a)
        {
            const Point<int16_t> at = base + position;
            GraphicsGL::get().drawrectangle(
                at.x(), at.y(), size.x(), size.y(), r, g, b, a
            );
        }

        // Filled rectangle whose height spans y0..y1 (inclusive of y1).
        inline void fill_band(Point<int16_t> base, int16_t x0, int16_t x1,
            int16_t y0, int16_t y1, float r, float g, float b, float a)
        {
            fill_rect(
                base,
                Point<int16_t>(x0, y0),
                Point<int16_t>(static_cast<int16_t>(x1 - x0 + 1),
                    static_cast<int16_t>(y1 - y0 + 1)),
                r, g, b, a
            );
        }

        // Rectangular outline assembled from four fills.
        inline void outline_rect(Point<int16_t> base, Point<int16_t> position,
            Point<int16_t> size, float r, float g, float b, float a,
            int16_t thickness = 1)
        {
            const int16_t width  = size.x();
            const int16_t height = size.y();
            fill_rect(base, position,
                Point<int16_t>(width, thickness), r, g, b, a);
            fill_rect(
                base,
                Point<int16_t>(
                    position.x(),
                    static_cast<int16_t>(position.y() + height - thickness)),
                Point<int16_t>(width, thickness), r, g, b, a);
            fill_rect(base, position,
                Point<int16_t>(thickness, height), r, g, b, a);
            fill_rect(
                base,
                Point<int16_t>(
                    static_cast<int16_t>(position.x() + width - thickness),
                    position.y()),
                Point<int16_t>(thickness, height), r, g, b, a);
        }

        // Horizontal accent line covering x0..x1 on row y.
        inline void hline(Point<int16_t> base, int16_t x0, int16_t x1,
            int16_t y, float r, float g, float b, float a,
            int16_t thickness = 1)
        {
            fill_band(base, x0, x1, y,
                static_cast<int16_t>(y + thickness - 1), r, g, b, a);
        }
    }
}
