// Native checks for the 2023 (modern) character select chrome geometry.
//
// CharSelectModernLayout.h describes the whole 800x600 screen with pure
// constants: a full-width top bar above the world banner anchor (y42), a
// left stage band behind the four-slot roster, a right info card with a
// header row and three data rows, a START button under the card and a
// footer band behind the create/delete buttons. NX art is only an accent
// layer, so these checks pin the chrome contracts:
//   - the top bar stays on screen and above the banner anchor, with the
//     slot-count fallback anchored inside it;
//   - every roster hotzone (and the selection outline inflated by
//     SLOT_SELECT_MARGIN) stays inside the stage band, and the page
//     arrows plus every feet anchor sit in the band too;
//   - the info card never overlaps the stage band, all four rows and
//     every label anchor stay inside the card, header bottom before the
//     job row, START strictly below the card;
//   - the footer keeps both button anchors inside it and on screen;
//   - the roster keeps its 120px slot pitch with the nametag still in
//     the stage band.
#include "IO/CharacterSelect/CharSelectModernLayout.h"
#include "Template/Rectangle.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
    using namespace jrc;
    using namespace jrc::charselect_modern;

    constexpr int16_t SCREEN_WIDTH = 800;
    constexpr int16_t SCREEN_HEIGHT = 600;

    constexpr Rectangle<int16_t> SCREEN_RECT{ 0, SCREEN_WIDTH, 0, SCREEN_HEIGHT };

    // The layout stores panels as a top-left position plus a size; the
    // client Rectangle template is an (l, r, t, b) pair instead.
    constexpr Rectangle<int16_t> rect_from_pos_size(
        const Point<int16_t>& pos, const Point<int16_t>& size)
    {
        return { pos.x(), static_cast<int16_t>(pos.x() + size.x()),
                 pos.y(), static_cast<int16_t>(pos.y() + size.y()) };
    }

    constexpr bool rect_contains_point(
        const Rectangle<int16_t>& rect, const Point<int16_t>& point)
    {
        return point.x() >= rect.l() && point.x() <= rect.r()
            && point.y() >= rect.t() && point.y() <= rect.b();
    }

    constexpr bool rect_contains_rect(
        const Rectangle<int16_t>& outer, const Rectangle<int16_t>& inner)
    {
        return inner.l() >= outer.l() && inner.r() <= outer.r()
            && inner.t() >= outer.t() && inner.b() <= outer.b();
    }

    void check_topbar()
    {
        const Rectangle<int16_t> bar =
            rect_from_pos_size(TOPBAR_POS, TOPBAR_SIZE);

        assert(rect_contains_rect(SCREEN_RECT, bar));
        assert(bar.b() <= WORLD_BANNER_POS.y());
        assert(rect_contains_point(bar, COUNT_FALLBACK_POS));
        assert(rect_contains_point(SCREEN_RECT, WORLD_BANNER_POS));
    }

    void check_stage_and_roster()
    {
        const Rectangle<int16_t> stage =
            rect_from_pos_size(STAGE_POS, STAGE_SIZE);
        assert(rect_contains_rect(SCREEN_RECT, stage));

        const Point<int16_t> hotzone_dim = slot_hotzone_dim();
        for (size_t i = 0; i < PAGESIZE; ++i)
        {
            const Rectangle<int16_t> hotzone =
                rect_from_pos_size(slot_hotzone_pos(i), hotzone_dim);

            // Clickable slot area stays inside the translucent band.
            assert(rect_contains_rect(stage, hotzone));

            // The selected outline (hotzone inflated by its margin) too.
            const Rectangle<int16_t> outline{
                static_cast<int16_t>(hotzone.l() - SLOT_SELECT_MARGIN),
                static_cast<int16_t>(hotzone.r() + SLOT_SELECT_MARGIN),
                static_cast<int16_t>(hotzone.t() - SLOT_SELECT_MARGIN),
                static_cast<int16_t>(hotzone.b() + SLOT_SELECT_MARGIN)
            };
            assert(rect_contains_rect(stage, outline));

            // The feet anchor lands on its own hotzone and inside the band.
            const Point<int16_t> feet = char_slot_pos(i);
            assert(rect_contains_point(hotzone, feet));
            assert(rect_contains_point(stage, feet));
        }

        // Page arrows flank the roster row from inside the band.
        assert(rect_contains_point(stage, PAGE_ARROW_LEFT_POS));
        assert(rect_contains_point(stage, PAGE_ARROW_RIGHT_POS));
    }

    void check_card_panel()
    {
        const Rectangle<int16_t> card =
            rect_from_pos_size(CARD_POS, CARD_SIZE);
        const Rectangle<int16_t> stage =
            rect_from_pos_size(STAGE_POS, STAGE_SIZE);

        assert(rect_contains_rect(SCREEN_RECT, card));
        // The info card starts where the stage band ends: no overlap.
        assert(card.l() >= stage.r());
        assert(!card.overlaps(stage));

        // Header text anchors live in the header row, above its bottom.
        assert(rect_contains_point(card, CARD_NAME_POS));
        assert(rect_contains_point(card, CARD_LV_POS));
        assert(CARD_NAME_POS.y() >= card.t());
        assert(CARD_LV_POS.y() <= CARD_HEADER_BOTTOM);
        assert(CARD_HEADER_BOTTOM >= card.t() && CARD_HEADER_BOTTOM <= card.b());

        // The three data rows fill the card below the header, stacked
        // in order without overlapping each other.
        const int16_t row_height = CARD_JOB_ROW_SIZE.y();
        const Rectangle<int16_t> job_row =
            rect_from_pos_size(CARD_JOB_ROW_POS, CARD_JOB_ROW_SIZE);
        const Rectangle<int16_t> stat_row =
            rect_from_pos_size(CARD_STAT_ROW_POS, CARD_STAT_ROW_SIZE);
        const Rectangle<int16_t> stat_row2 = rect_from_pos_size(
            CARD_STAT_ROW2_POS, { CARD_STAT_ROW_SIZE.x(), row_height });

        assert(job_row.t() > CARD_HEADER_BOTTOM);
        assert(stat_row.t() >= job_row.b());
        assert(stat_row2.t() >= stat_row.b());
        assert(rect_contains_rect(card, job_row));
        assert(rect_contains_rect(card, stat_row));
        assert(rect_contains_rect(card, stat_row2));

        // Every card content label anchor stays inside the panel.
        for (int label = CARD_LABEL_JOB; label <= CARD_LABEL_LUK; ++label)
        {
            assert(rect_contains_point(
                card, card_label_pos(static_cast<size_t>(label))));
        }
    }

    void check_start_button()
    {
        const Rectangle<int16_t> card =
            rect_from_pos_size(CARD_POS, CARD_SIZE);

        assert(START_POS.y() > card.b());
        assert(rect_contains_point(SCREEN_RECT, START_POS));
    }

    void check_footer()
    {
        const Rectangle<int16_t> footer =
            rect_from_pos_size(FOOTER_POS, FOOTER_SIZE);

        assert(rect_contains_rect(SCREEN_RECT, footer));
        assert(footer.b() <= SCREEN_HEIGHT);
        assert(rect_contains_point(footer, CREATE_POS));
        assert(rect_contains_point(footer, DELETE_POS));
    }

    void check_roster_spacing()
    {
        const Rectangle<int16_t> stage =
            rect_from_pos_size(STAGE_POS, STAGE_SIZE);

        for (size_t i = 0; i < PAGESIZE; ++i)
        {
            const Point<int16_t> feet = char_slot_pos(i);

            assert(feet.x() >= 0 && feet.x() <= SCREEN_WIDTH);
            if (i > 0)
            {
                assert(feet.x() - char_slot_pos(i - 1).x() == 120);
            }
        }

        // The nametag hangs below the feet anchor but stays on stage.
        const int16_t nametag_y =
            static_cast<int16_t>(char_slot_pos(0).y() + NAMETAG_OFFSET.y());
        assert(nametag_y >= stage.t() && nametag_y <= stage.b());

        // The platform art sink keeps the disc face below the anchor.
        assert(PLATFORM_OFFSET.y() > 0);
        assert(NAMETAG_OFFSET.y() > PLATFORM_OFFSET.y());
    }
}

int main()
{
    static_assert(PAGESIZE == 4, "the roster is one row of four slots");
    static_assert(slot_hotzone_dim().x() == 60 && slot_hotzone_dim().y() == 120,
        "hotzones keep the shipped 60x120 click box");
    static_assert(TOPBAR_POS.y() + TOPBAR_SIZE.y() <= WORLD_BANNER_POS.y(),
        "top bar bottom edge stays above the world banner anchor");
    static_assert(CARD_POS.x() >= STAGE_POS.x() + STAGE_SIZE.x(),
        "info card starts at or after the stage band right edge");
    static_assert(char_slot_pos(PAGESIZE - 1).x()
            + slot_hotzone_dim().x() / 2 <= STAGE_POS.x() + STAGE_SIZE.x(),
        "last slot hotzone centre stays inside the stage band");

    check_topbar();
    check_stage_and_roster();
    check_card_panel();
    check_start_button();
    check_footer();
    check_roster_spacing();

    std::cout << "PASS charselect 2023 chrome geometry (800x600)\n";
    return 0;
}
