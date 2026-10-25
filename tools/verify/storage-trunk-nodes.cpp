// Standalone verifier for the storage (trunk) window node resolution and
// layout dispatch in UIStorageNodes.h.
//
// The shipped UI.nx is a post-Chaos dump that still carries the legacy
// list-style Trunk window (UIWindow.img/Trunk, 463x318 with `en` row art),
// so the legacy window keeps winning the generation probe. Both
// generations are implemented; this guards:
//   * trunk generation detection picks the legacy window when `en` exists;
//   * selection/meso/empty-slot lookups resolve against the shipped file
//     (the legacy Shop subtree does not exist in a modern UIWindow.img);
//   * the legacy list geometry (4 rows, 84/40/35, columns 6/237,
//     icon offsets, select shift, tab strips, button strip anchors);
//   * the post-Chaos grid metadata (layer sizes/origins, tab art,
//     button authored spots) decoded straight from UIWindow2.img/Trunk;
//   * the post-Chaos grid geometry constants against the backgrnd3
//     pixels: the socket seams must line up with sn::GRID_*_ORIGIN,
//     sn::GRID_COL_PITCH, sn::GRID_ROW_PITCH and sn::GRID_CELL_SIZE, and the tab /
//     meso furniture must land inside the bands carved into the art.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function
//       -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes
//       tools/verify/storage-trunk-nodes.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp
//       src/nlnx/nx.cpp src/client/includes/lz4.c
//       -o /tmp/storage-trunk-nodes
//   /tmp/storage-trunk-nodes assets/UI.nx

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "IO/UITypes/UIStorageNodes.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/file_impl.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include "lz4.h"

// Verifier-local bitmap implementation (metadata plus pixel decode); the
// real decoder lives in the client and bitmap.cpp is not linked here.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}

    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
    uint32_t bitmap::length() const { return 4u * m_width * m_height; }

    void const* bitmap::data() const
    {
        if (!m_file)
        {
            return nullptr;
        }

        _file_data* fd = static_cast<_file_data*>(m_file);
        static std::vector<char> out;
        out.resize(length() + 64);

        std::vector<char> src(length());
        if (!io_read(fd, src.data(), length(), m_offset + 4))
        {
            return nullptr;
        }

        LZ4_decompress_safe(src.data(), out.data(),
            static_cast<int>(length()), static_cast<int>(length()));
        return out.data();
    }
}

namespace nl
{
    namespace nx
    {
        // Defined in nx.cpp; keeps the opened files alive and returns the root.
        node add_file(std::string name);
    }
}

namespace
{
    namespace sn = jrc::storage_nodes;

    int failures = 0;

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        if (!ok)
        {
            ++failures;
        }
    }

    void check_eq(int64_t actual, int64_t expected, const std::string& what)
    {
        check(actual == expected, what + " (" + std::to_string(actual)
            + " == " + std::to_string(expected) + ")");
    }

    void check_bitmap(nl::node n, uint16_t w, uint16_t h, const std::string& what)
    {
        const bool ok = n.data_type() == nl::node::type::bitmap
            && n.get_bitmap().width() == w
            && n.get_bitmap().height() == h;
        check(ok, what + " resolves to " + std::to_string(w) + "x"
            + std::to_string(h) + " bitmap");
    }

    // A decoded layer plus its origin, addressed in window coordinates.
    struct Raster
    {
        const uint8_t* px;
        uint16_t width;
        int16_t origin_x;
        int16_t origin_y;
    };

    Raster raster_of(nl::node n)
    {
        nl::bitmap bmp = n.get_bitmap();
        return Raster{
            static_cast<const uint8_t*>(bmp.data()),
            bmp.width(),
            static_cast<int16_t>(n["origin"].x()),
            static_cast<int16_t>(n["origin"].y())
        };
    }

    uint8_t alpha_at(const Raster& r, int16_t x, int16_t y)
    {
        const int row = y - r.origin_y;
        const int col = x - r.origin_x;
        if (row < 0 || col < 0 || col >= r.width)
        {
            return 255;
        }

        return r.px[(row * r.width + col) * 4 + 3];
    }

    // Fraction of a horizontal scan line whose alpha stays below `limit`.
    double transparent_h(const Raster& r, int16_t x0, int16_t x1, int16_t y, uint8_t limit)
    {
        int hit = 0;
        int total = 0;
        for (int16_t x = x0; x <= x1; ++x)
        {
            ++total;
            if (alpha_at(r, x, y) < limit)
            {
                ++hit;
            }
        }

        return total ? hit * 1.0 / total : 0.0;
    }

    // Fraction of a vertical scan line whose alpha stays below `limit`.
    double transparent_v(const Raster& r, int16_t y0, int16_t y1, int16_t x, uint8_t limit)
    {
        int hit = 0;
        int total = 0;
        for (int16_t y = y0; y <= y1; ++y)
        {
            ++total;
            if (alpha_at(r, x, y) < limit)
            {
                ++hit;
            }
        }

        return total ? hit * 1.0 / total : 0.0;
    }

    // Both components of the baked-in authored spot of a button state
    // (the anchor is position - origin, so the authored spot is -origin).
    void check_authored(nl::node state, int16_t x, int16_t y, const std::string& what)
    {
        check_eq(-state["origin"].x(), x, what + " authored x");
        check_eq(-state["origin"].y(), y, what + " authored y");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " <UI.nx>\n";
        return 2;
    }

    nl::nx::ui = nl::nx::add_file(argv[1]);
    nl::node grid = nl::nx::ui["UIWindow2.img"]["Trunk"];

    std::cout << "trunk generation dispatch\n";
    check(sn::legacy(), "legacy trunk art detected (UIWindow.img/Trunk/en)");
    check(!sn::post_chaos(), "post-Chaos grid trunk not selected");
    check(static_cast<bool>(sn::src()["backgrnd"]), "trunk src() resolves");
    check_eq(sn::src()["backgrnd"].get_bitmap().width(), 463, "legacy frame is the 463px list window");
    check_eq(sn::src()["backgrnd"].get_bitmap().height(), 318, "legacy frame height");

    std::cout << "shared subtree lookups\n";
    check_bitmap(sn::selection(), 162, 35, "selection()");
    check_bitmap(sn::empty_slot(), 199, 35, "empty_slot() (Trunk/en)");
    check(sn::meso_icon().data_type() == nl::node::type::bitmap,
        "meso_icon() resolves");
    check_bitmap(nl::nx::ui["UIWindow2.img"]["Shop"]["meso"], 12, 12,
        "post-Chaos Shop/meso coin (12x12, origin-less)");

    std::cout << "legacy list geometry\n";
    check_eq(sn::rows_visible(), 4, "rows_visible()");
    check_eq(sn::grid_cols(), 1, "grid_cols() (single legacy column)");
    check_eq(sn::cells_visible(), 4, "cells_visible()");
    check_eq(sn::col_pitch(), 0, "col_pitch() (no legacy column pitch)");
    check_eq(sn::grid_scroll_step(), 1, "grid_scroll_step() (one row per notch)");
    check_eq(sn::row_top(), 84, "row_top()");
    check_eq(sn::row_step(), 40, "row_step()");
    check_eq(sn::row_height(), 35, "row_height()");
    check_eq(sn::left_row_x(), 6, "left_row_x()");
    check_eq(sn::right_row_x(), 237, "right_row_x()");
    check_eq(sn::row_width(), 199, "row_width()");
    check_eq(sn::left_icon_x(), 10, "left_icon_x()");
    check_eq(sn::right_icon_x(), 241, "right_icon_x()");
    check_eq(sn::icon_y(), 86, "icon_y()");
    check_eq(sn::selection_shift(), 37, "selection_shift()");
    check_eq(sn::empty_slot_shift().x(), 0, "empty_slot_shift().x()");
    check_eq(sn::empty_slot_shift().y(), 0, "empty_slot_shift().y()");
    check_eq(sn::tab_pos(false, 0).x(), 37, "left tab strip x");
    check_eq(sn::tab_pos(false, 4).x(), 173, "left tab strip last x");
    check_eq(sn::tab_pos(true, 0).x(), 251, "right tab strip x");
    check_eq(sn::tab_pos(true, 4).x(), 387, "right tab strip last x");
    check_eq(sn::tab_pos(false, 0).y(), 59, "tab strip y");
    check_eq(sn::get_pos().y(), 247, "action button strip y");
    check(sn::get_pos().x() > 0 && sn::exit_pos().y() >= 271,
        "action buttons on the parchment strip");
    check(!sn::has_mode_buttons(), "mode buttons hidden for legacy trunk");

    std::cout << "post-Chaos grid metadata\n";
    check_bitmap(grid["backgrnd"], 347, 414, "grid backgrnd (shadow layer)");
    check_bitmap(grid["backgrnd2"], 333, 400, "grid backgrnd2 (beige plate)");
    check_bitmap(grid["backgrnd3"], 331, 392, "grid backgrnd3 (socket layer)");
    check_bitmap(grid["disabled"], 33, 33, "grid disabled (empty socket)");
    check_bitmap(grid["FullBackgrnd"], 768, 414, "grid FullBackgrnd");
    check_eq(grid["backgrnd"]["origin"].x(), 0, "backgrnd origin x");
    check_eq(grid["backgrnd"]["origin"].y(), 0, "backgrnd origin y");
    check_eq(grid["backgrnd2"]["origin"].x(), -7, "backgrnd2 origin x");
    check_eq(grid["backgrnd2"]["origin"].y(), -7, "backgrnd2 origin y");
    check_eq(grid["backgrnd3"]["origin"].x(), -8, "backgrnd3 origin x");
    check_eq(grid["backgrnd3"]["origin"].y(), -11, "backgrnd3 origin y");
    check_eq(grid["disabled"]["origin"].x(), 0, "disabled origin x");
    check_eq(grid["disabled"]["origin"].y(), 0, "disabled origin y");
    for (int i = 0; i < 5; ++i)
    {
        const std::string suffix = std::to_string(i);
        const nl::node tab_disabled = grid["Tab"]["disabled"][suffix];
        check_bitmap(tab_disabled, 30, 17, "tab disabled " + suffix);
        check_authored(tab_disabled, static_cast<int16_t>(183 + 31 * i), 78,
            "tab disabled " + suffix);
        check_authored(grid["Tab"]["enabled"][suffix],
            static_cast<int16_t>(183 + 31 * i), 76,
            "tab enabled " + suffix + " (bottom-aligned)");
    }
    check_authored(grid["BtGet"]["normal"]["0"], 94, 36, "BtGet");
    check_authored(grid["BtPut"]["normal"]["0"], 255, 17, "BtPut");
    check_authored(grid["BtSort"]["normal"]["0"], 94, 55, "BtSort");
    check_authored(grid["BtExit"]["normal"]["0"], 94, 17, "BtExit");
    check_authored(grid["BtSlotEx"]["normal"]["0"], 94, 74, "BtSlotEx");
    check_authored(grid["BtInCoin"]["normal"]["0"], 184, 387, "BtInCoin");
    check_authored(grid["BtOutCoin"]["normal"]["0"], 11, 387, "BtOutCoin");
    check_authored(grid["BtFull"]["normal"]["0"], 320, 387, "BtFull");

    std::cout << "post-Chaos grid geometry vs backgrnd3 pixels\n";
    const Raster bg3 = raster_of(grid["backgrnd3"]);
    const int16_t row0 = sn::GRID_LEFT_ORIGIN.y();
    const int16_t origins[2] = { sn::GRID_LEFT_ORIGIN.x(), sn::GRID_RIGHT_ORIGIN.x() };
    for (int panel = 0; panel < 2; ++panel)
    {
        const int16_t ox = origins[panel];
        const int16_t x0 = static_cast<int16_t>(ox + 2);
        const int16_t x1 = static_cast<int16_t>(ox + sn::GRID_COLS * sn::GRID_COL_PITCH - 2);

        // Horizontal socket seams: one line above row 0, a two pixel seam
        // above every following row and a closing seam under row 7.
        check(transparent_h(bg3, x0, x1, static_cast<int16_t>(row0 - 1), 16) >= 0.95,
            "seam above first row (panel " + std::to_string(panel) + ")");
        for (int16_t r = 1; r < sn::GRID_ROWS; ++r)
        {
            const int16_t top = static_cast<int16_t>(row0 + r * sn::GRID_ROW_PITCH);
            check(transparent_h(bg3, x0, x1, static_cast<int16_t>(top - 2), 16) >= 0.95,
                "row seam " + std::to_string(r) + " top (panel " + std::to_string(panel) + ")");
            check(transparent_h(bg3, x0, x1, static_cast<int16_t>(top - 1), 16) >= 0.95,
                "row seam " + std::to_string(r) + " bottom (panel " + std::to_string(panel) + ")");
        }
        const int16_t bottom = static_cast<int16_t>(row0 + sn::GRID_ROWS * sn::GRID_ROW_PITCH - 2);
        check(transparent_h(bg3, x0, x1, bottom, 16) >= 0.95,
            "seam under last row (panel " + std::to_string(panel) + ")");

        // Vertical seams before columns 1..3 and opaque socket interiors.
        for (int16_t c = 1; c < sn::GRID_COLS; ++c)
        {
            const int16_t cs = static_cast<int16_t>(ox + c * sn::GRID_COL_PITCH);
            check(alpha_at(bg3, static_cast<int16_t>(cs - 3), static_cast<int16_t>(row0 + 16)) < 16,
                "column seam " + std::to_string(c) + " (panel " + std::to_string(panel) + ")");
            check(alpha_at(bg3, cs, static_cast<int16_t>(row0 + 16)) > 240,
                "column " + std::to_string(c) + " left edge opaque (panel " + std::to_string(panel) + ")");
        }
        for (int16_t c = 0; c < sn::GRID_COLS; ++c)
        {
            for (int16_t r = 0; r < sn::GRID_ROWS; ++r)
            {
                const int16_t cx = static_cast<int16_t>(ox + c * sn::GRID_COL_PITCH + sn::GRID_CELL_SIZE / 2);
                const int16_t cy = static_cast<int16_t>(row0 + r * sn::GRID_ROW_PITCH + sn::GRID_CELL_SIZE / 2);
                check(alpha_at(bg3, cx, cy) > 240,
                    "socket (" + std::to_string(c) + "," + std::to_string(r) + ") interior opaque (panel "
                    + std::to_string(panel) + ")");
            }
        }
    }

    std::cout << "post-Chaos window furniture bands\n";
    check(transparent_v(bg3, static_cast<int16_t>(row0 + 2),
        static_cast<int16_t>(row0 + sn::GRID_ROWS * sn::GRID_ROW_PITCH - 3), 150, 16) >= 0.98,
        "divider gutter x136..167 is carved out");
    check(transparent_h(bg3, 150, 320, 62, 16) >= 0.98,
        "tab band y51..73 is carved out for the tab strip");
    check(transparent_h(bg3, 0, 322, 364, 16) >= 0.98,
        "parchment strip starts below y364");
    check(alpha_at(bg3, 90, 372) > 240 && alpha_at(bg3, 250, 372) > 240,
        "meso plates opaque under both readouts");
    check(alpha_at(bg3, 170, 372) < 16 && alpha_at(bg3, 310, 372) < 16,
        "meso plate gaps stay transparent");

    std::cout << "post-Chaos layout against the constants\n";
    for (int i = 0; i < 5; ++i)
    {
        const int16_t tx = static_cast<int16_t>(sn::GRID_TAB_ORIGIN.x() + sn::GRID_TAB_PITCH * i);
        check(tx >= 147 && tx + 29 <= 322 && sn::GRID_TAB_ORIGIN.y() >= 51
            && sn::GRID_TAB_ORIGIN.y() + 16 <= 73,
            "tab " + std::to_string(i) + " rect inside the tab band");
        if (i > 0)
        {
            const int16_t prev = static_cast<int16_t>(sn::GRID_TAB_ORIGIN.x() + sn::GRID_TAB_PITCH * (i - 1));
            check(tx >= prev + 30, "tab " + std::to_string(i) + " does not overlap tab "
                + std::to_string(i - 1));
        }
    }
    for (int panel = 0; panel < 2; ++panel)
    {
        const int16_t gx = origins[panel];
        const int16_t gy = sn::GRID_LEFT_ORIGIN.y();
        struct Spot { int16_t x; int16_t y; int16_t w; int16_t h; const char* name; };
        const Spot visible[] = {
            { 94, 17, 64, 16, "BtExit" },
            { 94, 36, 64, 16, "BtGet" },
            { 94, 55, 64, 16, "BtSort" },
            { 255, 17, 75, 16, "BtPut" }
        };
        for (const Spot& spot : visible)
        {
            const bool disjoint = spot.y + spot.h <= gy
                || spot.x + spot.w <= gx
                || spot.x >= gx + sn::GRID_COLS * sn::GRID_COL_PITCH;
            check(disjoint, std::string(spot.name) + " stays clear of panel "
                + std::to_string(panel));
        }
    }
    check(sn::GRID_IN_COIN_POS.y() >= 364 && sn::GRID_IN_COIN_POS.y() + 15 <= 380,
        "in-coin button lands on the parchment strip");
    check(sn::GRID_OUT_COIN_POS.y() >= 364 && sn::GRID_OUT_COIN_POS.y() + 15 <= 380,
        "out-coin button lands on the parchment strip");
    check(sn::GRID_STORAGE_MESO_ICON_POS.x() >= 33 && sn::GRID_STORAGE_MESO_ICON_POS.x() + 11 <= 144
        && sn::GRID_STORAGE_MESO_ICON_POS.y() >= 365,
        "storage meso coin sits on its plate");
    check(sn::GRID_PLAYER_MESO_ICON_POS.x() >= 206 && sn::GRID_PLAYER_MESO_ICON_POS.x() + 11 <= 299
        && sn::GRID_PLAYER_MESO_ICON_POS.y() >= 365,
        "player meso coin sits on its plate");
    check(sn::GRID_STORAGE_MESO_LABEL_POS.x() >= 33 && sn::GRID_STORAGE_MESO_LABEL_POS.x() <= 144,
        "storage meso label anchored on its plate");
    check(sn::GRID_PLAYER_MESO_LABEL_POS.x() >= 206 && sn::GRID_PLAYER_MESO_LABEL_POS.x() <= 299,
        "player meso label anchored on its plate");

    if (failures > 0)
    {
        std::cerr << failures << " storage trunk node checks failed\n";
        return 1;
    }

    std::cout << "ALL CHECKS PASSED\n";
    return 0;
}
