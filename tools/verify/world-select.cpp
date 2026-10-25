#include <cstdint>
#include <iostream>
#include <string>

#include "IO/UITypes/WorldSelectPolicy.h"
#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// This verifier reads bitmap metadata only, so decoding pixel payloads is
// intentionally not linked.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}

    uint16_t bitmap::width() const
    {
        return m_width;
    }

    uint16_t bitmap::height() const
    {
        return m_height;
    }
}

int main(int argc, char** argv)
{
    static_assert(jrc::world_select::selectable_channel_count(0) == 0);
    static_assert(jrc::world_select::selectable_channel_count(1) == 1);
    static_assert(jrc::world_select::selectable_channel_count(3) == 1);
    static_assert(jrc::world_select::selected_channel_id() == 0);

    bool request_pending = false;
    if (request_pending || !jrc::world_select::begin_request(request_pending) ||
        jrc::world_select::begin_request(request_pending))
    {
        std::cerr << "FAIL world-select request gate is not idempotent\n";
        return 1;
    }
    jrc::world_select::recover_request(request_pending);
    if (request_pending || !jrc::world_select::begin_request(request_pending))
    {
        std::cerr << "FAIL world-select request gate cannot recover\n";
        return 1;
    }

    const std::string path = argc > 1 ? argv[1] : "UI.nx";
    nl::file file(path);
    nl::node channels = file.root()["Login.img"]["WorldSelect"]["BtChannel"];
    nl::node go_normal = channels["button:GoWorld"]["normal"]["0"];
    nl::node channel_normal = channels["button:0"]["normal"]["0"];
    nl::node window = channels["layer:bg"];
    // Leftover cells are patched with the first baked-disabled grid cell, so
    // that cell has to exist and its region has to sit inside the window
    // bitmap; index 2 is the first cell the window does not bake as available.
    nl::node template_cell = channels["button:2"]["normal"]["0"];
    if (go_normal.data_type() != nl::node::type::bitmap
        || channel_normal.data_type() != nl::node::type::bitmap
        || window.data_type() != nl::node::type::bitmap
        || template_cell.data_type() != nl::node::type::bitmap)
    {
        std::cerr << "FAIL missing world-select channel bitmap\n";
        return 1;
    }

    const auto go_origin = go_normal["origin"].get_vector();
    const auto go_bitmap = go_normal.get_bitmap();
    const auto channel_origin = channel_normal["origin"].get_vector();
    const auto channel_bitmap = channel_normal.get_bitmap();
    const auto cell_origin = template_cell["origin"].get_vector();
    const auto cell_bitmap = template_cell.get_bitmap();
    const auto window_bitmap = window.get_bitmap();

    // The cell origin is the offset from the bitmap top-left to the position
    // the cell is drawn at, so the negation locates the baked region.
    const int source_x = -cell_origin.first;
    const int source_y = -cell_origin.second;
    const int source_w = cell_bitmap.width();
    const int source_h = cell_bitmap.height();
    const bool source_inside = source_x >= 0 && source_y >= 0
        && source_x + source_w <= window_bitmap.width()
        && source_y + source_h <= window_bitmap.height();

    std::cout << "go_world_origin=" << go_origin.first << ',' << go_origin.second
              << " dimensions=" << go_bitmap.width() << 'x' << go_bitmap.height()
              << " channel_origin=" << channel_origin.first << ',' << channel_origin.second
              << " dimensions=" << channel_bitmap.width() << 'x' << channel_bitmap.height()
              << " window_dimensions=" << window_bitmap.width() << 'x' << window_bitmap.height()
              << " disabled_cell_origin=" << cell_origin.first << ',' << cell_origin.second
              << " dimensions=" << source_w << 'x' << source_h
              << " disabled_region=" << source_x << ',' << source_y
              << ' ' << source_w << 'x' << source_h
              << (source_inside ? " inside-window" : " OUTSIDE-WINDOW")
              << '\n';

    if (!source_inside || source_w <= 0 || source_h <= 0)
    {
        std::cerr << "FAIL disabled cell region does not fit the window bitmap\n";
        return 1;
    }
    return go_bitmap.width() > 0 && go_bitmap.height() > 0
        && channel_bitmap.width() > 0 && channel_bitmap.height() > 0 ? 0 : 1;
}
