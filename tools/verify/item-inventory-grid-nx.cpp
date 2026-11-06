// Standalone verifier for the item inventory window NX layout contract in
// UIItemInventoryNodes.h. Every assertion is "NX data == code constant"; if
// either side drifts (new UI.nx dump or edited constants) this goes red.
//
// The shipped UI.nx is a post-Chaos dump, so iteminv_nodes dispatches to
// UIWindow2.img/Item via the global marker (Login.img/WorldSelect/BtChannel).
// Window coordinates below are relative to the 172x335 backgrnd at (0,0):
// layers backgrnd org(0,0), backgrnd2 160x306 org(-6,-23), backgrnd3 158x255
// org(-7,-45); the socket grid baked into backgrnd3 pixels (6 rows of 33px
// at y 51/86/121/156/191/226, pitch 35; 4 columns of 33px at x 10/46/82/118,
// pitch 36; 2px/3px transparent seams); tabs Tab/enabled/i 30x19
// org(-9-31i,-26) and Tab/disabled/i 30x17 org(-9-31i,-28); Item.nx leaves
// use 8-digit zero-padded names (Etc/0403.img/04031008).
//
// The 1px x offset between the first NX column (x 10) and grid_origin().x()
// (11) is the socket art's 1px left border: slotpos() is the interior origin
// consumers rely on - the lock overlay is a 32x32 ColorBox at slotpos
// (UIItemInventory.cpp:190) and page_offset_at() rejects xoff < 1, skipping
// the border column. Rows carry no such border, so grid_origin().y() equals
// the first NX row top (51) exactly.
//
// Usage (from the repository root): compile with the flags/sources listed in
// tools/verify/index.md, then run `/tmp/item-inventory-grid-nx assets/UI.nx
// assets/Item.nx`.

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "IO/UITypes/UIItemInventoryNodes.h"

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
            return nullptr;

        _file_data* fd = static_cast<_file_data*>(m_file);
        static std::vector<char> out;
        out.resize(length() + 64);

        std::vector<char> src(length());
        if (!io_read(fd, src.data(), length(), m_offset + 4))
            return nullptr;

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
    namespace inv = jrc::iteminv_nodes;

    int failures = 0;
    int checks = 0;

    int16_t px(int v) { return static_cast<int16_t>(v); }

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        ++checks;
        failures += ok ? 0 : 1;
    }

    void check_eq(int64_t actual, int64_t expected, const std::string& what)
    {
        check(actual == expected, what + ": expected " + std::to_string(expected)
            + ", actual " + std::to_string(actual));
    }

    void check_bitmap(nl::node n, uint16_t w, uint16_t h, const std::string& what)
    {
        const nl::bitmap bmp = n.get_bitmap();
        check(n.data_type() == nl::node::type::bitmap && bmp.width() == w
            && bmp.height() == h, what + " resolves to " + std::to_string(w) + "x"
            + std::to_string(h) + " bitmap (actual " + std::to_string(bmp.width())
            + "x" + std::to_string(bmp.height()) + ")");
    }

    void check_origin(nl::node n, int ox, int oy, const std::string& what)
    {
        check_eq(n["origin"].x(), ox, what + " origin x");
        check_eq(n["origin"].y(), oy, what + " origin y");
    }

    // A decoded layer anchored in window space. MapleStory sprites draw at
    // position - origin, so the window-space top-left of a layer is -origin.
    struct Raster
    {
        const uint8_t* px;
        uint16_t width;
        uint16_t height;
        int16_t base_x;
        int16_t base_y;
    };

    Raster raster_of(nl::node n)
    {
        nl::bitmap bmp = n.get_bitmap();
        return Raster{static_cast<const uint8_t*>(bmp.data()), bmp.width(), bmp.height(),
            px(-n["origin"].x()), px(-n["origin"].y())};
    }

    uint8_t alpha_at(const Raster& r, int16_t x, int16_t y)
    {
        const int row = y - r.base_y;
        const int col = x - r.base_x;
        if (row < 0 || row >= r.height || col < 0 || col >= r.width)
            return 0;
        return r.px[(row * r.width + col) * 4 + 3];
    }

    // One opaque run along a scan line; `at` picks the free coordinate.
    struct Run
    {
        int16_t start;
        int16_t length;
    };

    using Sampler = uint8_t (*)(const Raster&, int16_t, int16_t);

    // Horizontal runs: fixed coordinate is the scan line's y.
    uint8_t alpha_h(const Raster& r, int16_t y, int16_t x) { return alpha_at(r, x, y); }

    // Vertical runs: fixed coordinate is the scan line's x.
    uint8_t alpha_v(const Raster& r, int16_t x, int16_t y) { return alpha_at(r, x, y); }

    std::vector<Run> opaque_runs(const Raster& r, Sampler at, int16_t fixed,
        int16_t from, int16_t to, int16_t min_len)
    {
        std::vector<Run> runs;
        int16_t start = 0;
        int16_t len = 0;
        for (int16_t v = from; v <= to + 1; ++v)
        {
            if (v <= to && at(r, fixed, v) > 128)
            {
                if (len == 0)
                    start = v;
                ++len;
            }
            else
            {
                if (len >= min_len)
                    runs.push_back(Run{start, len});
                len = 0;
            }
        }
        return runs;
    }

    std::string runs_str(const std::vector<Run>& runs)
    {
        std::string s;
        for (const Run& run : runs)
        {
            s += (s.empty() ? "" : "; ") + std::to_string(run.start)
                + "+" + std::to_string(run.length);
        }
        return s;
    }

    // True when `count` consecutive lines from `fixed` stay near-transparent
    // across [from, to] on the free coordinate.
    bool seams_clear(const Raster& r, Sampler at, int16_t fixed, int count,
        int16_t from, int16_t to)
    {
        for (int d = 0; d < count; ++d)
        {
            for (int16_t v = from; v <= to; ++v)
            {
                if (at(r, px(fixed + d), v) > 16)
                {
                    return false;
                }
            }
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cout << "usage: " << argv[0] << " <UI.nx> <Item.nx>\n";
        return 2;
    }


    nl::nx::ui = nl::nx::add_file(argv[1]);
    nl::nx::item = nl::nx::add_file(argv[2]);
    nl::node win = nl::nx::ui["UIWindow2.img"]["Item"];

    std::cout << "generation dispatch\n";
    check(inv::post_chaos(), "global marker picks the post-Chaos asset set");
    check(inv::has_layered_background(), "layered background selected");
    check_bitmap(inv::src()["backgrnd2"], 160, 306, "src() resolves UIWindow2.img/Item");

    std::cout << "window frame layers\n";
    check_eq(inv::dimension().x(), 172, "dimension().x()");
    check_eq(inv::dimension().y(), 335, "dimension().y()");
    check_bitmap(win["backgrnd"], 172, 335, "backgrnd");
    check_origin(win["backgrnd"], 0, 0, "backgrnd");
    check_bitmap(win["backgrnd2"], 160, 306, "backgrnd2");
    check_origin(win["backgrnd2"], -6, -23, "backgrnd2");
    check_bitmap(win["backgrnd3"], 158, 255, "backgrnd3");
    check_origin(win["backgrnd3"], -7, -45, "backgrnd3");
    check(6 + 160 <= inv::dimension().x() && 23 + 306 <= inv::dimension().y(),
        "backgrnd2 rect (anchored at -origin) stays inside the frame");
    check(7 + 158 <= inv::dimension().x() && 45 + 255 <= inv::dimension().y(),
        "backgrnd3 rect (anchored at -origin) stays inside the frame");

    // Socket grid, measured straight off the backgrnd3 pixels.
    const Raster bg3 = raster_of(win["backgrnd3"]);
    check_eq(bg3.base_x, 7, "backgrnd3 anchored at window x7");
    check_eq(bg3.base_y, 45, "backgrnd3 anchored at window y45");
    const int16_t grid_w = px(inv::grid_origin().x() + 15);
    const int16_t grid_h = px(inv::grid_origin().y() + 16);
    const std::vector<Run> cols = opaque_runs(bg3, alpha_h, grid_h, 0,
        px(inv::dimension().x() - 1), 30);
    const std::vector<Run> rows = opaque_runs(bg3, alpha_v, grid_w, 0,
        px(inv::dimension().y() - 1), 30);
    const bool grid_found = cols.size() == 4 && rows.size() == 6;
    check(grid_found, "backgrnd3 carries exactly 4 columns x 6 rows (columns "
        + runs_str(cols) + "; rows " + runs_str(rows) + ")");

    if (grid_found)
    {
        for (int c = 0; c < 4; ++c)
        {
            check_eq(cols[c].start, inv::grid_origin().x() - 1 + c * inv::column_pitch(),
                "column " + std::to_string(c) + " left border");
            check_eq(cols[c].length, 33, "column " + std::to_string(c) + " cell width");
        }
        for (int r = 0; r < 6; ++r)
        {
            check_eq(rows[r].start, inv::grid_origin().y() + r * inv::row_pitch(),
                "row " + std::to_string(r) + " top");
            check_eq(rows[r].length, 33, "row " + std::to_string(r) + " cell height");
        }
        // Seam scans stay inside the cell interiors: column border pixels
        // can poke into the row seam (x82 at y84 here) - art, not drift.
        for (int r = 0; r < 5; ++r)
        {
            bool clear = true;
            for (int c = 0; c < 4; ++c)
                clear = clear && seams_clear(bg3, alpha_h,
                    px(rows[r].start + rows[r].length), 2,
                    px(cols[c].start + 2), px(cols[c].start + 30));
            check(clear, "2px seam under row " + std::to_string(r) + " (cell interiors)");
        }
        for (int c = 0; c < 3; ++c)
        {
            check(seams_clear(bg3, alpha_v, px(cols[c].start + cols[c].length), 3,
                px(rows[0].start + 2), px(rows[5].start + 31)),
                "3px seam right of column " + std::to_string(c));
        }
        int holes = 0;
        for (int r = 0; r < 6; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                if (alpha_at(bg3, px(cols[c].start + 16), px(rows[r].start + 16)) <= 240)
                    ++holes;
            }
        }
        check_eq(holes, 0, "all 24 socket interiors opaque");

        std::cout << "code constants vs the measured grid\n";
        check_eq(inv::grid_origin().x(), cols[0].start + 1,
            "grid_origin().x() is the first column interior (NX left border + 1px)");
        check_eq(inv::grid_origin().y(), rows[0].start,
            "grid_origin().y() equals the first row top");
        check_eq(cols[0].length - 1, 32,
            "code cell interior is 32px wide (33px art - 1px border)");
        int slot_misses = 0;
        int hit_misses = 0;
        for (int16_t s = 0; s < 24; ++s)
        {
            const jrc::Point<int16_t> pos = inv::slotpos(s);
            if (pos.x() != px(cols[s % 4].start + 1) || pos.y() != rows[s / 4].start)
                ++slot_misses;
            if (inv::page_offset_at(jrc::Point<int16_t>(px(cols[s % 4].start + 16),
                px(rows[s / 4].start + 16))) != s)
                ++hit_misses;
        }
        check_eq(slot_misses, 0, "slotpos() equals every measured cell interior");
        check_eq(hit_misses, 0, "page_offset_at() maps every socket center");
        check_eq(inv::page_offset_at(jrc::Point<int16_t>(
            px(inv::grid_origin().x() - 7), grid_h)), -1,
            "page_offset_at() rejects the left margin");
        check_eq(inv::page_offset_at(jrc::Point<int16_t>(grid_w,
            px(inv::grid_origin().y() - 7))), -1,
            "page_offset_at() rejects above the grid");
        check_eq(inv::page_offset_at(jrc::Point<int16_t>(
            px(inv::dimension().x() - 12), grid_h)), -1,
            "page_offset_at() rejects past the scrollbar");
    }

    std::cout << "tab strip\n";
    for (int i = 0; i < 5; ++i)
    {
        const std::string idx = std::to_string(i);
        nl::node en = win["Tab"]["enabled"][idx];
        nl::node dis = win["Tab"]["disabled"][idx];
        check_bitmap(en, 30, 19, "Tab/enabled/" + idx);
        check_origin(en, -9 - 31 * i, -26, "Tab/enabled/" + idx);
        check_bitmap(dis, 30, 17, "Tab/disabled/" + idx);
        check_origin(dis, -9 - 31 * i, -28, "Tab/disabled/" + idx);
        const int16_t en_bottom = px(inv::tab_button_pos(i).y() - en["origin"].y() + 19);
        const int16_t dis_bottom = px(inv::tab_button_pos(i).y() - dis["origin"].y() + 17);
        const int16_t authored_x = px(inv::tab_button_pos(i).x() - en["origin"].x());
        check(en_bottom == dis_bottom, "tab " + idx + " enabled/disabled bottoms align at y "
            + std::to_string(en_bottom));
        check(en_bottom <= inv::grid_origin().y(), "tab " + idx + " sits above the grid");
        check(authored_x >= 0 && authored_x + 30 <= inv::dimension().x(), "tab " + idx
            + " inside the window (authored x " + std::to_string(authored_x) + ")");
    }

    check(inv::drop_meso_pos().x() == 0 && inv::drop_meso_pos().y() == 0,
        "post-Chaos drop_meso_pos() is (0,0); art origins carry the spots");
    nl::node coin = win["BtCoin"]["normal"]["0"];
    check_bitmap(coin, 40, 16, "BtCoin normal frame");
    check_origin(coin, -9, -267, "BtCoin normal frame (authored at (9,267))");
    if (grid_found)
    {
        std::cout << "scrollbar and meso furniture\n";
        check_eq(inv::slider_x(), cols[3].start + cols[3].length + 1,
            "slider_x() one pixel past the last socket's right border");
        check_eq(inv::slider_vertical().first(), inv::grid_origin().y() - 1,
            "slider track starts one pixel above the grid");
        int opaque = 0;
        for (int16_t x = inv::slider_x(); x < bg3.base_x + bg3.width; ++x)
        {
            for (int16_t y = inv::grid_origin().y(); y < rows[5].start + 33; ++y)
                if (alpha_at(bg3, x, y) > 16)
                    ++opaque;
        }
        check_eq(opaque, 0, "socket layer leaves the scrollbar strip empty");
        check(-coin["origin"].y() >= rows[5].start + 33,
            "meso coin sits below the last socket row");
    }

    std::cout << "representative item icon (Item.nx)\n";
    nl::node leaf = nl::nx::item["Etc"]["0403.img"]["04031008"];
    check(leaf && leaf.size() > 0,
        "8-digit zero-padded leaf 04031008 resolves under Etc/0403.img");
    nl::node icon = leaf["info"]["icon"];
    check_bitmap(icon, 29, 31, "04031008 info/icon");
    check_origin(icon, -2, 31, "04031008 info/icon");
    check(icon.get_bitmap().width() <= 32 && icon.get_bitmap().height() <= 32,
        "icon fits the 32px code socket interior (33px NX cell - 1px border)");
    check_eq(icon["origin"].y(), icon.get_bitmap().height(),
        "icon is bottom-anchored (origin.y == height)");

    if (failures > 0)
    {
        std::cerr << failures << " item inventory grid checks failed\n";
        return 1;
    }

    std::cout << "PASS: item inventory grid contract holds (" << checks
        << " checks; UIWindow2.img/Item " << inv::dimension().x() << "x"
        << inv::dimension().y() << ", 6x4 sockets 33x33 @ pitch "
        << inv::row_pitch() << "/" << inv::column_pitch() << ", grid_origin ("
        << inv::grid_origin().x() << "," << inv::grid_origin().y() << "))\n";
    return 0;
}
