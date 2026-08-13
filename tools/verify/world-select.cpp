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
    nl::node normal = file.root()["Login.img"]["WorldSelect"]["BtChannel"]
        ["button:GoWorld"]["normal"]["0"];
    if (normal.data_type() != nl::node::type::bitmap)
    {
        std::cerr << "FAIL missing GoWorld normal bitmap\n";
        return 1;
    }

    const auto origin = normal["origin"].get_vector();
    const auto bitmap = normal.get_bitmap();
    std::cout << "go_world_origin=" << origin.first << ',' << origin.second
              << " dimensions=" << bitmap.width() << 'x' << bitmap.height()
              << '\n';
    return bitmap.width() > 0 && bitmap.height() > 0 ? 0 : 1;
}
