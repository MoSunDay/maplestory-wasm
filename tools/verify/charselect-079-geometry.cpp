// Native checks for the 079MAX4 bitmap character-select geometry.
//
// CharSelect079Layout.h pins the classic 800x600 screen: backdrop and frame
// anchor on the canvas centre (400,300), the 161x188 charInfo1 card hangs off
// INFO_ANCHOR (625,305), four discs carry the roster and the banner/counter/
// buttons/page arrows live in the frame margins. These checks pin:
//   - frame/backdrop cover the canvas exactly; the interior is well ordered;
//   - the card lands on its baked origin (45,57) inside the interior;
//   - the documented A11M/A12M glyph-top math for the name and the nine
//     value rows, the baked job bitmap top-left, every value column inside
//     the card and the left column left of the right one;
//   - rank_icon_pos: 4px right of the centred value, y snapped by height;
//   - the 120px roster pitch, hotzones clear of the card, disc on screen;
//   - banner/counter inside the interior, counter in the right margin;
//   - buttons in the bottom band create < delete < start without overlap,
//     page arrows below the roster and symmetric about x=400.
#include "IO/CharacterSelect/CharSelect079Layout.h"
#include "Template/Rectangle.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace
{
    using namespace jrc;
    using namespace jrc::charselect_079;

    constexpr int16_t SCREEN_WIDTH = 800;
    constexpr int16_t SCREEN_HEIGHT = 600;

    constexpr Rectangle<int16_t> SCREEN_RECT{ 0, SCREEN_WIDTH, 0, SCREEN_HEIGHT };

    // The layout stores art as a position plus a size; the client Rectangle
    // template is an (l, r, t, b) pair instead.
    constexpr Rectangle<int16_t> rect_from_pos_size(
        const Point<int16_t>& pos, const Point<int16_t>& size)
    {
        return { pos.x(), static_cast<int16_t>(pos.x() + size.x()),
                 pos.y(), static_cast<int16_t>(pos.y() + size.y()) };
    }

    // Sprite rect for a bitmap anchored by the given origin.
    constexpr Rectangle<int16_t> rect_from_origin(
        const Point<int16_t>& pos, const Point<int16_t>& origin,
        const Point<int16_t>& size)
    {
        return rect_from_pos_size(
            { static_cast<int16_t>(pos.x() - origin.x()),
              static_cast<int16_t>(pos.y() - origin.y()) }, size);
    }

    // Sprite rect for a bitmap anchored on its own centre.
    constexpr Rectangle<int16_t> rect_from_center(
        const Point<int16_t>& centre, const Point<int16_t>& size)
    {
        return rect_from_pos_size(
            { static_cast<int16_t>(centre.x() - size.x() / 2),
              static_cast<int16_t>(centre.y() - size.y() / 2) }, size);
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

    // Shift a point by a delta (Point's own operator+/- narrow through int,
    // which trips -Wnarrowing under -Werror).
    constexpr Point<int16_t> point_shift(
        const Point<int16_t>& pos, const Point<int16_t>& delta)
    {
        return { static_cast<int16_t>(pos.x() + delta.x()),
                 static_cast<int16_t>(pos.y() + delta.y()) };
    }

    // The frame's transparent interior, as documented by the header.
    constexpr Rectangle<int16_t> FRAME_INNER{
        FRAME_INNER_LEFT, FRAME_INNER_RIGHT,
        FRAME_INNER_TOP, FRAME_INNER_BOTTOM
    };

    // charInfo1 is 161x188 with a baked origin of (45,57); anchoring it puts
    // its top-left at INFO_TOPLEFT.
    constexpr Point<int16_t> INFO_CARD_SIZE{ 161, 188 };
    constexpr Point<int16_t> INFO_CARD_ORIGIN{ 45, 57 };
    // Anchor -> top-left, i.e. minus the baked origin.
    constexpr Point<int16_t> INFO_CARD_OFFSET{
        static_cast<int16_t>(-INFO_CARD_ORIGIN.x()),
        static_cast<int16_t>(-INFO_CARD_ORIGIN.y())
    };
    constexpr Rectangle<int16_t> INFO_CARD =
        rect_from_pos_size(INFO_TOPLEFT, INFO_CARD_SIZE);

    // Text::A11M has bitmap top offset 3 on a 14px line, so its draw y is
    // the glyph top minus 11; A12M offsets 3 on a 17px line (-14).
    constexpr int16_t A11M_GLYPH_OFFSET = 11;
    constexpr int16_t A12M_GLYPH_OFFSET = 14;

    // The 150x57 world banner board injected as Login.img/selectWorld079.
    constexpr Point<int16_t> WORLD_BANNER_SIZE{ 150, 57 };
    // The slot counter lives against the right edge of the frame interior.
    constexpr int16_t COUNTER_RIGHT_MARGIN = 40;

    // The three 079 button bitmaps (BtNew/BtDelete/BtSelect) are 94x43.
    constexpr Point<int16_t> BUTTON_SIZE{ 94, 43 };

    // character/0 is the 60x9 disc already sitting at the feet anchor.
    constexpr Point<int16_t> DISC_SIZE{ 60, 9 };
    constexpr Point<int16_t> DISC_ORIGIN{ 29, 7 };

    // Card value rows in draw order, used for the in-card column check.
    constexpr Point<int16_t> VALUE_ROWS[] = {
        INFO_JOB_POS, INFO_LEVEL_POS, INFO_FAME_POS, INFO_STR_POS,
        INFO_INT_POS, INFO_DEX_POS, INFO_LUK_POS, INFO_RANK_POS,
        INFO_JOBRANK_POS
    };
    constexpr size_t VALUE_ROW_COUNT = sizeof(VALUE_ROWS) / sizeof(VALUE_ROWS[0]);

    void check_canvas_and_frame()
    {
        const Rectangle<int16_t> canvas = rect_from_pos_size({ 0, 0 }, CANVAS_SIZE);
        assert(canvas.l() == SCREEN_RECT.l() && canvas.r() == SCREEN_RECT.r());
        assert(canvas.t() == SCREEN_RECT.t() && canvas.b() == SCREEN_RECT.b());

        // Backdrop and frame both anchor on the canvas centre, so the
        // 800x600 frame sprite lands exactly on the canvas rect.
        const Point<int16_t> centre{ SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
        assert(FRAME_POS == centre);
        assert(BACKDROP_POS == centre);

        const Rectangle<int16_t> frame = rect_from_center(FRAME_POS, CANVAS_SIZE);
        assert(frame.l() == 0 && frame.t() == 0);
        assert(frame.r() == SCREEN_WIDTH && frame.b() == SCREEN_HEIGHT);

        // The transparent interior is a non-degenerate window inside it.
        assert(FRAME_INNER_LEFT < FRAME_INNER_RIGHT);
        assert(FRAME_INNER_TOP < FRAME_INNER_BOTTOM);
        assert(rect_contains_rect(SCREEN_RECT, FRAME_INNER));
        assert(FRAME_INNER.l() > SCREEN_RECT.l());
        assert(FRAME_INNER.r() < SCREEN_RECT.r());
        assert(FRAME_INNER.t() > SCREEN_RECT.t());
        assert(FRAME_INNER.b() < SCREEN_RECT.b());
    }

    void check_info_card()
    {
        // Anchoring charInfo1 by its baked origin reproduces the top-left.
        assert(INFO_TOPLEFT == point_shift(INFO_ANCHOR, INFO_CARD_OFFSET));

        assert(INFO_CARD.r() - INFO_CARD.l() == INFO_CARD_SIZE.x());
        assert(INFO_CARD.b() - INFO_CARD.t() == INFO_CARD_SIZE.y());
        assert(rect_contains_rect(SCREEN_RECT, INFO_CARD));

        // The whole card hangs inside the frame interior.
        assert(rect_contains_rect(FRAME_INNER, INFO_CARD));
        assert(INFO_CARD.l() > FRAME_INNER_LEFT);
        assert(INFO_CARD.r() < FRAME_INNER_RIGHT);
        assert(INFO_CARD.t() > FRAME_INNER_TOP);
        assert(INFO_CARD.b() < FRAME_INNER_BOTTOM);
    }

    // A11M row: draw y = card top + local glyph top - top offset.
    void check_a11m_row(Point<int16_t> pos, int16_t glyph_top)
    {
        assert(pos.y() == INFO_TOPLEFT.y() + glyph_top - A11M_GLYPH_OFFSET);

        // The glyph face, not the draw origin, lands on the caption band.
        const int16_t glyph_y =
            static_cast<int16_t>(pos.y() + A11M_GLYPH_OFFSET);
        assert(glyph_y == INFO_TOPLEFT.y() + glyph_top);
        assert(glyph_y >= INFO_CARD.t() && glyph_y <= INFO_CARD.b());
    }

    void check_info_text_rows()
    {
        // A12M name centred in the blue header, glyph top at local y5.
        assert(INFO_NAME_POS.y() - INFO_TOPLEFT.y() == 5 - A12M_GLYPH_OFFSET);

        // Draw y is the A12M origin, so the glyph face (not the anchor)
        // lands on the header band at local y5.
        const int16_t name_glyph_y =
            static_cast<int16_t>(INFO_NAME_POS.y() + A12M_GLYPH_OFFSET);
        assert(name_glyph_y == INFO_TOPLEFT.y() + 5);
        assert(name_glyph_y >= INFO_CARD.t() && name_glyph_y <= INFO_CARD.b());

        // The nine A11M values, glyph tops measured on the baked captions.
        check_a11m_row(INFO_JOB_POS, 26);
        check_a11m_row(INFO_LEVEL_POS, 45);
        check_a11m_row(INFO_FAME_POS, 45);
        check_a11m_row(INFO_STR_POS, 64);
        check_a11m_row(INFO_INT_POS, 64);
        check_a11m_row(INFO_DEX_POS, 83);
        check_a11m_row(INFO_LUK_POS, 83);
        check_a11m_row(INFO_RANK_POS, 121);
        check_a11m_row(INFO_JOBRANK_POS, 159);

        // The job-name bitmap is drawn at its own top-left, no glyph offset.
        assert(INFO_JOB_ICON_POS
            == point_shift(INFO_TOPLEFT, Point<int16_t>(44, 26)));
        assert(rect_contains_point(INFO_CARD, INFO_JOB_ICON_POS));

        // Every value anchor stays inside the card and the rows stack
        // downwards in caption order.
        for (size_t i = 0; i < VALUE_ROW_COUNT; ++i)
        {
            assert(rect_contains_point(INFO_CARD, VALUE_ROWS[i]));
            if (i > 0)
            {
                assert(VALUE_ROWS[i].y() >= VALUE_ROWS[i - 1].y());
            }
        }

        // Left column is left of the right column; both rank values use the
        // centred column of the light bands.
        assert(INFO_LEVEL_POS.x() == INFO_STR_POS.x());
        assert(INFO_STR_POS.x() == INFO_DEX_POS.x());
        assert(INFO_FAME_POS.x() == INFO_INT_POS.x());
        assert(INFO_INT_POS.x() == INFO_LUK_POS.x());
        assert(INFO_LEVEL_POS.x() < INFO_FAME_POS.x());
        assert(INFO_RANK_POS.x() == INFO_JOBRANK_POS.x());
    }

    void check_rank_icon_pos()
    {
        const Point<int16_t> base{ 100, 200 };

        // Arrow rides 4px right of the centred value. Icons at least 9px
        // tall take the 11px offset.
        const Point<int16_t> row_icon = rank_icon_pos(base, 24, 9);
        assert(row_icon.x() == base.x() + 24 / 2 + 4);
        assert(row_icon.y() == base.y() + 11);
        assert(row_icon == Point<int16_t>(116, 211));

        // Taller icons (11px/17px) keep the same 11px offset.
        const Point<int16_t> tall = rank_icon_pos(base, 24, 17);
        assert(tall.x() == base.x() + 24 / 2 + 4);
        assert(tall.y() == base.y() + 11);

        // Anything shorter drops to the 14px offset; 8px is the boundary.
        assert(rank_icon_pos(base, 24, 8).y() == base.y() + 14);
        assert(rank_icon_pos(base, 24, 8) == Point<int16_t>(116, 214));
        assert(rank_icon_pos(base, 0, 8) == Point<int16_t>(104, 214));

        // Odd widths keep the integer half-width rule.
        assert(rank_icon_pos(base, 7, 9) == Point<int16_t>(107, 211));

        // Real rows: the arrow stays right of the centred value glyphs and
        // follows the same height snap.
        const Point<int16_t> rank = rank_icon_pos(INFO_RANK_POS, 24, 9);
        assert(rank.x() > INFO_RANK_POS.x());
        assert(rank.y() == INFO_RANK_POS.y() + 11);
        const Point<int16_t> jobrank = rank_icon_pos(INFO_JOBRANK_POS, 24, 11);
        assert(jobrank.x() > INFO_JOBRANK_POS.x());
        assert(jobrank.y() == INFO_JOBRANK_POS.y() + 11);
    }

    void check_roster()
    {
        const Point<int16_t> hotzone_dim = slot_hotzone_dim();
        assert(hotzone_dim.x() < 120);

        for (size_t i = 0; i < PAGESIZE; ++i)
        {
            const Point<int16_t> feet = char_slot_pos(i);
            const Rectangle<int16_t> disc =
                rect_from_origin(feet, DISC_ORIGIN, DISC_SIZE);
            const Rectangle<int16_t> hotzone =
                rect_from_pos_size(slot_hotzone_pos(i), hotzone_dim);

            // Feet anchor and hotzone stay inside the frame interior.
            assert(rect_contains_point(FRAME_INNER, feet));
            assert(rect_contains_rect(FRAME_INNER, hotzone));
            assert(rect_contains_point(hotzone, feet));

            // The click box never reaches the info card.
            assert(!hotzone.overlaps(INFO_CARD));
            assert(hotzone.r() < INFO_CARD.l());

            // The disc is centred on the anchor and stays on screen.
            assert(rect_contains_point(disc, feet));
            assert(rect_contains_rect(SCREEN_RECT, disc));

            if (i > 0)
            {
                assert(feet.x() - char_slot_pos(i - 1).x() == 120);
                assert(slot_hotzone_pos(i).x() - slot_hotzone_pos(i - 1).x() == 120);
            }
        }
    }

    void check_banner_and_counter()
    {
        const Rectangle<int16_t> banner =
            rect_from_pos_size(WORLD_BANNER_POS, WORLD_BANNER_SIZE);

        // The 150x57 world board hangs inside the frame, above the card.
        assert(rect_contains_rect(FRAME_INNER, banner));
        assert(!banner.overlaps(INFO_CARD));
        assert(banner.b() < INFO_CARD.t());

        // The slot counter sits in the frame's right margin, clear of the
        // banner and above the card header.
        assert(rect_contains_point(FRAME_INNER, COUNT_POS));
        assert(COUNT_POS.x() >= FRAME_INNER_RIGHT - COUNTER_RIGHT_MARGIN);
        assert(COUNT_POS.x() <= FRAME_INNER_RIGHT);
        assert(COUNT_POS.y() < INFO_CARD.t());
        assert(!rect_contains_point(banner, COUNT_POS));
    }

    void check_buttons_and_arrows()
    {
        const Rectangle<int16_t> create = rect_from_pos_size(CREATE_POS, BUTTON_SIZE);
        const Rectangle<int16_t> del = rect_from_pos_size(DELETE_POS, BUTTON_SIZE);
        const Rectangle<int16_t> start = rect_from_pos_size(START_POS, BUTTON_SIZE);
        const Rectangle<int16_t> buttons[] = { create, del, start };

        for (const Rectangle<int16_t>& button : buttons)
        {
            // Inside the frame's horizontal span, in the bottom band below
            // the interior, and never off screen.
            assert(button.l() >= FRAME_INNER_LEFT);
            assert(button.r() <= FRAME_INNER_RIGHT);
            assert(button.t() > FRAME_INNER_BOTTOM);
            assert(rect_contains_rect(SCREEN_RECT, button));
        }

        // create < delete < start, with no art overlap.
        assert(CREATE_POS.x() < DELETE_POS.x());
        assert(DELETE_POS.x() < START_POS.x());
        assert(create.r() <= del.l());
        assert(del.r() <= start.l());
        assert(!create.overlaps(del));
        assert(!del.overlaps(start));

        // Page arrows flank the roster: inside the interior's x-span, below
        // the last hotzone, symmetric about x=400 within 2px.
        const int16_t roster_bottom =
            static_cast<int16_t>(slot_hotzone_pos(0).y() + slot_hotzone_dim().y());
        for (const Point<int16_t>& arrow : { PAGE_LEFT_POS, PAGE_RIGHT_POS })
        {
            assert(arrow.x() >= FRAME_INNER_LEFT);
            assert(arrow.x() <= FRAME_INNER_RIGHT);
            assert(arrow.y() > roster_bottom);
            assert(arrow.y() >= FRAME_INNER_BOTTOM);
            assert(rect_contains_point(SCREEN_RECT, arrow));
        }

        assert(PAGE_LEFT_POS.x() < PAGE_RIGHT_POS.x());
        const int16_t left_gap =
            static_cast<int16_t>(SCREEN_WIDTH / 2 - PAGE_LEFT_POS.x());
        const int16_t right_gap =
            static_cast<int16_t>(PAGE_RIGHT_POS.x() - SCREEN_WIDTH / 2);
        assert(left_gap - right_gap <= 2);
        assert(right_gap - left_gap <= 2);
    }
}

int main()
{
    static_assert(PAGESIZE == 4, "the roster is one row of four slots");
    static_assert(CANVAS_SIZE.x() == 800 && CANVAS_SIZE.y() == 600,
        "the 079 art is authored for the classic 800x600 login canvas");
    static_assert(FRAME_POS == BACKDROP_POS,
        "frame and backdrop share the canvas centre");
    static_assert(INFO_TOPLEFT == point_shift(INFO_ANCHOR, INFO_CARD_OFFSET),
        "charInfo1 lands on its baked origin");
    static_assert(slot_hotzone_dim().x() == 60 && slot_hotzone_dim().y() == 120,
        "hotzones keep the shipped 60x120 click box");
    static_assert(INFO_NAME_POS.y() - INFO_TOPLEFT.y() == 5 - 14,
        "A12M name draw y puts the glyph top on local y5");
    static_assert(CREATE_POS.x() + 94 <= DELETE_POS.x()
            && DELETE_POS.x() + 94 <= START_POS.x(),
        "the 94px button art never overlaps");

    check_canvas_and_frame();
    check_info_card();
    check_info_text_rows();
    check_rank_icon_pos();
    check_roster();
    check_banner_and_counter();
    check_buttons_and_arrows();

    std::cout << "PASS charselect 079 bitmap geometry (800x600)\n";
    return 0;
}
