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

    const std::string path = argc > 1 ? argv[1] : "UI.nx";
    nl::file file(path);
    nl::node channels = file.root()["Login.img"]["WorldSelect"]["BtChannel"];
    nl::node go_normal = channels["button:GoWorld"]["normal"]["0"];
    nl::node channel_normal = channels["button:0"]["normal"]["0"];
    if (go_normal.data_type() != nl::node::type::bitmap
        || channel_normal.data_type() != nl::node::type::bitmap)
    {
        std::cerr << "FAIL missing world-select button bitmap\n";
        return 1;
    }

    const auto go_origin = go_normal["origin"].get_vector();
    const auto go_bitmap = go_normal.get_bitmap();
    const auto channel_origin = channel_normal["origin"].get_vector();
    const auto channel_bitmap = channel_normal.get_bitmap();
    std::cout << "go_world_origin=" << go_origin.first << ',' << go_origin.second
              << " dimensions=" << go_bitmap.width() << 'x' << go_bitmap.height()
              << " channel_origin=" << channel_origin.first << ',' << channel_origin.second
              << " dimensions=" << channel_bitmap.width() << 'x' << channel_bitmap.height()
              << '\n';
    return go_bitmap.width() > 0 && go_bitmap.height() > 0
        && channel_bitmap.width() > 0 && channel_bitmap.height() > 0 ? 0 : 1;
}
