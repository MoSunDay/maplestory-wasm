// Native checks for the shared chat-bar / status-bar hit geometry.
//
// UIChatbar draws its log at x = 0 in both UI generations: 502px wide
// post-Chaos, 566px (the shipped base/chat art) pre-Chaos. Pre-Chaos
// puts its input row inside the 71px v83 strip; post-Chaos floats it in a
// 65px gap above the bar anchor. Both reduce to the same ChatLayout, and
// the status bar may only claim clicks that land on a real control or on
// that open log.
#include "IO/UITypes/StatusBar/ChatBarGeometry.h"

#include <cassert>
#include <iostream>

namespace
{
    using namespace jrc;
    using namespace jrc::chatbar_geometry;

    void check_postchaos_800x600()
    {
        const ChatLayout layout =
            chat_layout_for_screen(Generation::POSTCHAOS, 600, 4);
        const Rectangle<int16_t> box = chatbox_rect(layout);

        assert(layout.anchor_y == 590);
        assert(chat_top(layout) == 461);
        assert(box.l() == 0 && box.r() == 502);
        assert(box.t() == 461 && box.b() == 590);
        assert(box.b() - box.t() == chatbox_height(layout));
        assert(box.width() == CHATBOX_WIDTH);

        // The black log body keeps its 2px handle inset and 1px separator.
        const Rectangle<int16_t> body = log_body_rect(layout);
        assert(body.t() == 463 && body.b() == 528);
        assert(body.height() == 1 + 4 * CHATROWHEIGHT);

        const Rectangle<int16_t> handle = drag_handle_rect(layout);
        assert(handle.t() == box.t() - 6 && handle.b() == box.t() + 6);
        assert(handle.l() == box.l() && handle.r() == box.r());
    }

    void check_prechaos_800x600()
    {
        const ChatLayout layout =
            chat_layout_for_screen(Generation::PRECHAOS, 600, 4);
        const Rectangle<int16_t> box = chatbox_rect(layout);

        assert(layout.anchor_y == 590);
        assert(chat_top(layout) == 455);
        assert(layout.width == PRECHAOS_CHAT_WIDTH);
        assert(box.l() == 0 && box.r() == 566);
        assert(box.t() == 455 && box.b() == 590);
        assert(box.b() - box.t() == chatbox_height(layout));

        const Rectangle<int16_t> body = log_body_rect(layout);
        assert(body.t() == 457 && body.b() == 522);
    }

    void check_1280x720()
    {
        const ChatLayout post =
            chat_layout_for_screen(Generation::POSTCHAOS, 720, 4);
        const Rectangle<int16_t> postbox = chatbox_rect(post);
        assert(post.anchor_y == 710);
        assert(postbox.t() == 581 && postbox.b() == 710);

        const ChatLayout pre =
            chat_layout_for_screen(Generation::PRECHAOS, 720, 4);
        const Rectangle<int16_t> prebox = chatbox_rect(pre);
        assert(pre.anchor_y == 710);
        assert(prebox.t() == 575 && prebox.b() == 710);

        // Bar anchors follow the shared helper for both generations.
        assert(bar_top(Generation::PRECHAOS, 720) == 639);
        assert(bar_top(Generation::POSTCHAOS, 720) == 625);
        assert(chatbar_anchor(Generation::POSTCHAOS, 720).x()
            == POSTCHAOS_LEFT_ANCHOR_X);
        assert(chatbar_anchor(Generation::POSTCHAOS, 720).y() == 710);
    }

    void check_collapsed_state()
    {
        const ChatLayout layout =
            chat_layout_for_screen(Generation::POSTCHAOS, 600, 4);

        // Inside the log, but collapsed: inert.
        assert(!chatbox_hit(false, layout, { 10, 500 }));
        assert(chatbox_hit(true, layout, { 10, 500 }));

        // Outside the 502px log in every state.
        assert(!chatbox_hit(true, layout, { 503, 500 }));
        assert(!chatbox_hit(true, layout, { 10, 460 }));
        assert(!chatbox_hit(true, layout, { 10, 591 }));

        // Row clamping cannot produce an empty / runaway log.
        assert(chat_layout_for_screen(
            Generation::POSTCHAOS, 600, 0).rows == MINCHATROWS);
        assert(chat_layout_for_screen(
            Generation::POSTCHAOS, 600, 99).rows == MAXCHATROWS);
    }

    void check_resize_drag()
    {
        // One row per 16px of travel, truncated toward zero.
        assert(rows_after_drag(4, 500, 484) == 5);
        assert(rows_after_drag(4, 500, 516) == 3);
        assert(rows_after_drag(4, 500, 491) == 4);
        assert(rows_after_drag(4, 500, 509) == 4);

        // Long drags clamp at MIN/MAX rows in either direction.
        assert(rows_after_drag(4, 500, 200) == MAXCHATROWS);
        assert(rows_after_drag(4, 500, 900) == MINCHATROWS);
        assert(rows_after_drag(16, 500, 100) == MAXCHATROWS);
        assert(rows_after_drag(1, 500, 900) == MINCHATROWS);
    }

    void check_statusbar_union()
    {
        const ChatLayout layout =
            chat_layout_for_screen(Generation::POSTCHAOS, 600, 4);
        const Rectangle<int16_t> box = chatbox_rect(layout);

        // Representative right-cluster button at 800x600 (BtSystem).
        HitTest controls;
        add_hit_rect(controls, { 512, 567, 538, 573 });

        assert(hit_any(controls, { 520, 540 }));
        assert(!hit_any(controls, { 600, 550 }));

        // Button hit: claimed with the chat collapsed or open.
        assert(statusbar_hit_test(controls, false, box, { 520, 540 }));
        assert(statusbar_hit_test(controls, true, box, { 520, 540 }));

        // Open chat log: claimed.
        assert(statusbar_hit_test(controls, true, box, { 10, 500 }));
        // Collapsed chat log: falls through.
        assert(!statusbar_hit_test(controls, false, box, { 10, 500 }));

        // Blank status-bar background: never claimed, even with the chat
        // open, so world clicks are not swallowed by the HUD strip.
        assert(!statusbar_hit_test(controls, false, box, { 600, 550 }));
        assert(!statusbar_hit_test(controls, true, box, { 600, 550 }));

        // Capacity guard keeps the fixed HitTest bounded.
        for (std::size_t i = 0; i < HitTest::CAPACITY + 4; ++i)
        {
            add_hit_rect(controls, { 0, 1, 0, 1 });
        }
        assert(controls.count == HitTest::CAPACITY);
    }
}

int main()
{
    static_assert(CHATBOX_WIDTH == 502,
        "chat log width must be one value for draw and hit");
    static_assert(chat_width(Generation::PRECHAOS) == 566
            && chat_width(Generation::POSTCHAOS) == 502,
        "chat log width is 566 pre-Chaos and 502 post-Chaos");
    static_assert(chatbox_rect({ 590, 4, POSTCHAOS_CHAT_TAIL }).t() == 461,
        "post-chaos 800x600 log top");
    static_assert(chatbox_rect({ 590, 4, PRECHAOS_CHAT_TAIL }).t() == 455,
        "pre-chaos 800x600 log top");

    check_postchaos_800x600();
    check_prechaos_800x600();
    check_1280x720();
    check_collapsed_state();
    check_resize_drag();
    check_statusbar_union();

    std::cout << "PASS chatbar/statusbar hit geometry (800x600, 1280x720)\n";
    return 0;
}
