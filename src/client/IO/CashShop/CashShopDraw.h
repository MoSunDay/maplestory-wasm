//////////////////////////////////////////////////////////////////////////////
// Shared drawing helpers for the cash shop screen.
//
// Layout constants use the 800x600 design canvas. The scene scales that
// canvas onto the viewport, centres it, and then applies VIEWYOFFSET, so
// overlays, hitboxes, and artwork must all route through these helpers.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "CashShopLayout.h"
#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Template/Point.h"

namespace jrc
{
namespace cash_shop_draw
{
    namespace layout = cash_shop_layout;

    inline Point<int16_t> draw_position(Point<int16_t> position)
    {
        return layout::origin() + layout::scale_point(position) -
            Point<int16_t>{ 0, Constants::VIEWYOFFSET };
    }

    inline Point<int16_t> draw_size(Point<int16_t> size)
    {
        return layout::scale_point(size);
    }

    inline void fill_rect(Point<int16_t> position, Point<int16_t> size,
        float r, float g, float b, float a)
    {
        const Point<int16_t> drawn = draw_position(position);
        const Point<int16_t> scaled = draw_size(size);
        GraphicsGL::get().drawrectangle(
            drawn.x(), drawn.y(), scaled.x(), scaled.y(), r, g, b, a);
    }

    inline void outline_rect(Point<int16_t> position, Point<int16_t> size,
        float r, float g, float b, float a, int16_t thickness = 1)
    {
        const int16_t width = size.x();
        const int16_t height = size.y();
        fill_rect(position, { width, thickness }, r, g, b, a);
        fill_rect(
            { position.x(),
                static_cast<int16_t>(
                    position.y() + height - thickness) },
            { width, thickness }, r, g, b, a);
        fill_rect(position, { thickness, height }, r, g, b, a);
        fill_rect(
            { static_cast<int16_t>(
                position.x() + width - thickness), position.y() },
            { thickness, height }, r, g, b, a);
    }

    inline Point<int16_t> currency_label_position(size_t index)
    {
        return layout::pill_label(
            layout::currency_position(index), layout::CURRENCY_SIZE);
    }

    inline Point<int16_t> tab_label_position(Point<int16_t> tab_position)
    {
        return layout::pill_label(tab_position, layout::TAB_SIZE);
    }
}
}
