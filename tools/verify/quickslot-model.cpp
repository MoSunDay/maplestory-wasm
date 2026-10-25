#include "IO/UITypes/StatusBar/QuickSlotModel.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
    using jrc::Point;
    using jrc::quickslot::COUNT;
    using jrc::quickslot::Layout;
    using jrc::quickslot::State;

    int16_t at(const Layout& layout, int16_t x, int16_t y)
    {
        return jrc::quickslot::index_at(layout, Point<int16_t>(x, y));
    }

    void verify_centers(const Layout& layout)
    {
        for (int16_t i = 0; i < COUNT; ++i)
        {
            const Point<int16_t> center =
                jrc::quickslot::key_center(layout, i);
            assert(at(layout, center.x(), center.y()) == i);
        }

        // Out-of-range indices have no center.
        assert(jrc::quickslot::key_center(layout, -1) == Point<int16_t>());
        assert(jrc::quickslot::key_center(layout, COUNT) == Point<int16_t>());
    }

    void verify_boundaries(const Layout& layout)
    {
        const int16_t x0 = layout.column_left.front();
        const int16_t x1 = layout.column_right.back();
        const int16_t y0 = layout.row_top.front();
        const int16_t y1 = layout.row_bottom.back();

        // Low edges are inside, high edges and one pixel beyond are not.
        assert(at(layout, x0, y0) == 0);
        assert(at(layout, x0, layout.row_top[1] - 1) == 0);
        assert(at(layout, layout.column_left[1], y0) == 1);
        assert(at(layout, x1 - 1, y1 - 1) == COUNT - 1);
        assert(at(layout, x0 - 1, y0) == -1);
        assert(at(layout, x1, y0) == -1);
        assert(at(layout, x0, y0 - 1) == -1);
        assert(at(layout, x0, y1) == -1);
    }

    void verify_layouts()
    {
        using jrc::quickslot::CHAOS_LAYOUT;
        using jrc::quickslot::V83_LAYOUT;

        // Measured panels: v83 151x80, post-Chaos 145x93.
        assert(V83_LAYOUT.dimension == Point<int16_t>(151, 80));
        assert(CHAOS_LAYOUT.dimension == Point<int16_t>(145, 93));
        assert(!V83_LAYOUT.labels_outside_slots);
        assert(CHAOS_LAYOUT.labels_outside_slots);

        // Post-Chaos keycaps live in the 16px header and 14px base bands,
        // clear of both slot rows.
        assert(CHAOS_LAYOUT.key_label_y[0] < CHAOS_LAYOUT.row_top[0]);
        assert(CHAOS_LAYOUT.key_label_y[1] > CHAOS_LAYOUT.row_bottom[1]);

        // The post-Chaos slot grid is shifted by the 16px header band and
        // the wider separators; its hit bands must not alias onto v83's.
        assert(CHAOS_LAYOUT.row_top[0] == 16);
        assert(CHAOS_LAYOUT.row_top[1] == 46);
        assert(at(CHAOS_LAYOUT, 3, 7) == -1);
    }

    void verify_texture_draw_position()
    {
        // Post-Chaos panel: 145x93 with NX origin (-143,143). Texture::draw
        // subtracts that origin, so the draw position has to pre-add it to
        // make the bitmap land on the intended top-left.
        const Point<int16_t> origin(-143, 143);
        const Point<int16_t> top_left(1135, 557);
        const Point<int16_t> draw_pos =
            jrc::quickslot::texture_draw_position(top_left, origin);

        assert(draw_pos.x() - origin.x() == top_left.x());
        assert(draw_pos.y() - origin.y() == top_left.y());
        assert(draw_pos == Point<int16_t>(1135 - 143, 557 + 143));

        // The old `panel.draw(top_left)` skipped the shift: the bitmap ended
        // up one whole origin away from the hit-test/icons/keycaps.
        assert(draw_pos != top_left);

        // Zero origin textures are drawn at their top-left unchanged.
        assert(jrc::quickslot::texture_draw_position(
            top_left, Point<int16_t>()) == top_left);
    }

    void verify_invalid_indices()
    {
        State state;
        assert(jrc::quickslot::skill_at(state, -1) == 0);
        assert(jrc::quickslot::skill_at(state, COUNT) == 0);
        assert(!jrc::quickslot::assign(state, -1, 1001004));
        assert(!jrc::quickslot::assign(state, COUNT, 1001004));

        // Clearing an invalid index is a no-op, not a crash.
        jrc::quickslot::clear(state, -1);
        jrc::quickslot::clear(state, COUNT);
        for (int16_t i = 0; i < COUNT; ++i)
        {
            assert(jrc::quickslot::skill_at(state, i) == 0);
            assert(jrc::quickslot::item_at(state, i) == 0);
        }
    }

    void verify_assign_and_clear()
    {
        State state;
        assert(state.hovered == -1);
        assert(state.press_id == 0);

        assert(jrc::quickslot::assign(state, 3, 1001004));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        // Bad skill ids are rejected and leave the stored value untouched.
        assert(!jrc::quickslot::assign(state, 3, 0));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);
        assert(!jrc::quickslot::assign(state, 3, -5));
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        // Slots are independent.
        assert(jrc::quickslot::assign(state, 5, 2001002));
        assert(jrc::quickslot::skill_at(state, 5) == 2001002);
        assert(jrc::quickslot::skill_at(state, 3) == 1001004);

        jrc::quickslot::clear(state, 3);
        assert(jrc::quickslot::skill_at(state, 3) == 0);
        assert(jrc::quickslot::skill_at(state, 5) == 2001002);

        jrc::quickslot::clear(state, 5);
        assert(jrc::quickslot::skill_at(state, 5) == 0);
    }

    void verify_item_bindings()
    {
        State state;
        assert(jrc::quickslot::item_at(state, -1) == 0);
        assert(jrc::quickslot::item_at(state, COUNT) == 0);
        assert(!jrc::quickslot::assign_item(state, -1, 2000000));
        assert(!jrc::quickslot::assign_item(state, 2, 0));

        assert(jrc::quickslot::assign_item(state, 2, 2000000));
        assert(jrc::quickslot::item_at(state, 2) == 2000000);
        assert(jrc::quickslot::skill_at(state, 2) == 0);

        // Skills and items share one slot: assigning either clears the
        // other content kind.
        assert(jrc::quickslot::assign(state, 2, 1001004));
        assert(jrc::quickslot::skill_at(state, 2) == 1001004);
        assert(jrc::quickslot::item_at(state, 2) == 0);

        assert(jrc::quickslot::assign_item(state, 2, 2000000));
        assert(jrc::quickslot::item_at(state, 2) == 2000000);
        assert(jrc::quickslot::skill_at(state, 2) == 0);

        jrc::quickslot::clear(state, 2);
        assert(jrc::quickslot::item_at(state, 2) == 0);
    }
}

int main()
{
    verify_layouts();
    verify_centers(jrc::quickslot::V83_LAYOUT);
    verify_centers(jrc::quickslot::CHAOS_LAYOUT);
    verify_boundaries(jrc::quickslot::V83_LAYOUT);
    verify_boundaries(jrc::quickslot::CHAOS_LAYOUT);
    verify_texture_draw_position();
    verify_invalid_indices();
    verify_assign_and_clear();
    verify_item_bindings();

    std::cout << "PASS quickslot model (both layouts: hit bands, centers,"
        " key labels, texture draw position, skill/item assign, clear)\n";
    return 0;
}
