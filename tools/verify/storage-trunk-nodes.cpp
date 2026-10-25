// Standalone verifier for the storage (trunk) window node resolution and
// generation dispatch in UIStorageNodes.h.
//
// The shipped UI.nx is a post-Chaos dump that still carries the legacy
// list-style Trunk window (UIWindow.img/Trunk, 463x318 with `en` row art).
// The post-Chaos UIWindow2.img Trunk is a 347x414 icon grid whose layout is
// not implemented, so the legacy window must win. This guards:
//   * trunk generation detection picks the legacy window when `en` exists;
//   * selection/meso/empty-slot lookups resolve against the shipped file
//     (the legacy Shop subtree does not exist in a modern UIWindow.img);
//   * the legacy list geometry (4 rows, 84/40/35, columns 6/237,
//     icon offsets, select shift, tab strips, button strip anchors).
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

#include "IO/UITypes/UIStorageNodes.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

// Metadata-only bitmap implementation; the real decoder lives in the client.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
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
    namespace storage_nodes = jrc::storage_nodes;

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
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " <UI.nx>\n";
        return 2;
    }

    nl::nx::ui = nl::nx::add_file(argv[1]);

    std::cout << "trunk generation dispatch\n";
    check(storage_nodes::legacy(), "legacy trunk art detected (UIWindow.img/Trunk/en)");
    check(!storage_nodes::post_chaos(), "post-Chaos grid trunk not selected");
    check(static_cast<bool>(storage_nodes::src()["backgrnd"]), "trunk src() resolves");
    check_eq(storage_nodes::src()["backgrnd"].get_bitmap().width(), 463, "legacy frame is the 463px list window");
    check_eq(storage_nodes::src()["backgrnd"].get_bitmap().height(), 318, "legacy frame height");

    std::cout << "shared subtree lookups\n";
    check_bitmap(storage_nodes::selection(), 162, 35, "selection()");
    check_bitmap(storage_nodes::empty_slot(), 199, 35, "empty_slot() (Trunk/en)");
    check(storage_nodes::meso_icon().data_type() == nl::node::type::bitmap,
        "meso_icon() resolves");

    std::cout << "legacy list geometry\n";
    check_eq(storage_nodes::rows_visible(), 4, "rows_visible()");
    check_eq(storage_nodes::row_top(), 84, "row_top()");
    check_eq(storage_nodes::row_step(), 40, "row_step()");
    check_eq(storage_nodes::row_height(), 35, "row_height()");
    check_eq(storage_nodes::left_row_x(), 6, "left_row_x()");
    check_eq(storage_nodes::right_row_x(), 237, "right_row_x()");
    check_eq(storage_nodes::row_width(), 199, "row_width()");
    check_eq(storage_nodes::left_icon_x(), 10, "left_icon_x()");
    check_eq(storage_nodes::right_icon_x(), 241, "right_icon_x()");
    check_eq(storage_nodes::icon_y(), 86, "icon_y()");
    check_eq(storage_nodes::selection_shift(), 37, "selection_shift()");
    check_eq(storage_nodes::empty_slot_shift().x(), 0, "empty_slot_shift().x()");
    check_eq(storage_nodes::empty_slot_shift().y(), 0, "empty_slot_shift().y()");
    check_eq(storage_nodes::tab_pos(false, 0).x(), 37, "left tab strip x");
    check_eq(storage_nodes::tab_pos(false, 4).x(), 173, "left tab strip last x");
    check_eq(storage_nodes::tab_pos(true, 0).x(), 251, "right tab strip x");
    check_eq(storage_nodes::tab_pos(true, 4).x(), 387, "right tab strip last x");
    check_eq(storage_nodes::tab_pos(false, 0).y(), 59, "tab strip y");
    check_eq(storage_nodes::get_pos().y(), 247, "action button strip y");
    check(storage_nodes::get_pos().x() > 0 && storage_nodes::exit_pos().y() >= 271,
        "action buttons on the parchment strip");
    check(!storage_nodes::has_mode_buttons(), "mode buttons hidden for legacy trunk");

    if (failures > 0)
    {
        std::cerr << failures << " storage trunk node checks failed\n";
        return 1;
    }

    std::cout << "ALL CHECKS PASSED\n";
    return 0;
}
