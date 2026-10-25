// c++ -std=c++17 -Isrc/client -Isrc tools/verify/cash-shop-scale.cpp -o /tmp/cash-shop-scale && /tmp/cash-shop-scale
#include "Graphics/DrawArgument.h"
#include "IO/CashShop/CashShopLayout.h"

#include <cmath>
#include <iostream>

namespace
{
    bool ok = true;

    bool same(jrc::Point<int16_t> lhs, jrc::Point<int16_t> rhs)
    {
        return lhs.x() == rhs.x() && lhs.y() == rhs.y();
    }

    bool same(jrc::Rectangle<int16_t> lhs, jrc::Rectangle<int16_t> rhs)
    {
        return same(lhs.getlt(), rhs.getlt()) &&
            same(lhs.getrb(), rhs.getrb());
    }

    void check(bool condition, const char* message)
    {
        if (condition)
        {
            std::cout << "OK " << message << '\n';
        }
        else
        {
            ok = false;
            std::cerr << "FAIL " << message << '\n';
        }
    }

    // Mirrors the pure draw helpers in IO/CashShop/CashShopDraw.h: design
    // coordinates are scaled onto the enlarged canvas, centred, then offset
    // by VIEWYOFFSET.
    jrc::Point<int16_t> draw_position(jrc::Point<int16_t> position)
    {
        return jrc::cash_shop_layout::origin() +
            jrc::cash_shop_layout::scale_point(position) -
            jrc::Point<int16_t>{ 0, jrc::Constants::VIEWYOFFSET };
    }

    jrc::Point<int16_t> draw_size(jrc::Point<int16_t> size)
    {
        return jrc::cash_shop_layout::scale_point(size);
    }
}

int main()
{
    using namespace jrc;
    namespace layout = cash_shop_layout;
    using Constants::set_viewsize;

    // 800x600: the design canvas is drawn 1:1 and pinned to the origin.
    set_viewsize(800, 600);
    check(layout::content_scale() == 1.0f, "800x600 scale is exactly 1");
    check(same(layout::origin(), {0, 0}), "800x600 origin is {0,0}");
    const Rectangle<int16_t> hitbox_800{{100, 34}, {471, 59}};
    check(same(layout::search_hitbox(), hitbox_800),
        "800x600 search hitbox unchanged");
    check(layout::scroll_split_x() == 504,
        "800x600 scroll split stays 504");
    check(same(layout::scale_point({800, 600}), {800, 600}),
        "800x600 scale_point is identity");

    // 1280x720 preset: the canvas scales uniformly by 1.2 and is centred.
    set_viewsize(1280, 720);
    check(std::fabs(layout::content_scale() - 1.2f) < 1e-4f,
        "1280x720 scale is 1.2");
    check(same(layout::origin(), {160, 0}),
        "1280x720 origin is {160,0}");
    // Rounding coverage drives off the live search hitbox edges so these
    // checks keep their pure-function intent without dead design constants.
    const Point<int16_t> hitbox_lt = layout::SEARCH_HITBOX.getlt();
    const Point<int16_t> hitbox_rb = layout::SEARCH_HITBOX.getrb();
    check(layout::scale_coord(hitbox_lt.x()) == 120,
        "scale_coord rounds the hitbox left edge (100) to 120");
    check(layout::scale_coord(hitbox_lt.y()) == 41,
        "scale_coord rounds the hitbox top edge (34) up to 41");
    check(layout::scale_coord(hitbox_rb.x()) == 565,
        "scale_coord rounds the hitbox right edge (471) down to 565");
    const Rectangle<int16_t> hitbox_1280{{280, 41}, {725, 71}};
    check(same(layout::search_hitbox(), hitbox_1280),
        "1280x720 search hitbox is scaled and centred");
    check(layout::scroll_split_x() == 765,
        "1280x720 scroll split is 765");
    check(same(layout::scale_point({800, 600}), {960, 720}),
        "1280x720 canvas scales to 960x720");
    check(layout::origin().x() + layout::scale_coord(800) <= 1280 &&
        layout::origin().y() + layout::scale_coord(600) <= 720,
        "scaled canvas fits the viewport");

    // DrawArgument rectangles mirror the fixed UICashShop call shapes: the
    // native texture origin is added after draw_position() so it stays
    // unscaled, because get_rectangle() subtracts it in native units.
    // Icon case: a 32x40 glyph centred on a point, native origin {17,-23}.
    const Point<int16_t> center{400, 300};
    const Point<int16_t> icon_origin{17, -23};
    const Point<int16_t> dimensions{32, 40};
    const Point<int16_t> size = draw_size(dimensions);
    const DrawArgument icon_arg{
        draw_position(center) - size / 2 + icon_origin, size};
    check(same(
              icon_arg.get_rectangle(icon_origin, dimensions).getlt(),
              draw_position(center) - size / 2),
        "icon top-left keeps the native origin out of the scaling");
    check(same(icon_arg.get_rectangle(icon_origin, dimensions).getrb(),
        draw_position(center) + size / 2),
        "icon rectangle spans the scaled size around the centre");
    // Action-button case: a 36x28 design button, native origin {9,14}.
    const Point<int16_t> action_pos{120, 540};
    const Point<int16_t> action_origin{9, 14};
    const Point<int16_t> button_size{36, 28};
    const DrawArgument action_arg{
        draw_position(action_pos) + action_origin,
        draw_size(button_size)};
    check(same(
              action_arg.get_rectangle(action_origin, {72, 56}).getlt(),
              draw_position(action_pos)),
        "action button top-left adds the native origin after scaling");
    // The old formula scaled the origin and the half-size a second time.
    const DrawArgument buggy_icon_arg{
        draw_position(center - size / 2 + icon_origin), size};
    check(!same(
              buggy_icon_arg.get_rectangle(icon_origin, dimensions).getlt(),
              draw_position(center) - size / 2),
        "old icon formula double-scales the origin at 1.2x");

    // Restore the default viewport for later checks.
    set_viewsize(800, 600);
    check(layout::content_scale() == 1.0f && same(layout::origin(), {0, 0}),
        "viewsize resets back to the 800x600 design grid");

    if (ok)
        std::cout << "PASS cash shop scale adaptation\n";
    return ok ? 0 : 1;
}
