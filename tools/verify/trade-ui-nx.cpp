#include <cstdint>
#include <iostream>
#include <string>

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    bool bitmap_is(nl::node node, uint16_t width = 0, uint16_t height = 0)
    {
        if (node.data_type() != nl::node::type::bitmap)
            return false;
        const auto image = node.get_bitmap();
        return image.width() > 0 && image.height() > 0
            && (width == 0 || image.width() == width)
            && (height == 0 || image.height() == height);
    }

    bool button_is_complete(nl::node button)
    {
        const char* states[] = { "normal", "pressed", "disabled", "mouseOver" };
        for (const char* state : states)
            if (!bitmap_is(button[state]["0"]))
                return false;
        return true;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "UI.nx";
    nl::file ui(path);
    nl::node room = ui.root()["UIWindow2.img"]["CashTradingRoom"];
    bool ok = bitmap_is(room["backgrnd"], 654, 566)
        && bitmap_is(room["backgrnd2"])
        && bitmap_is(room["backgrnd3"])
        && button_is_complete(room["BtClame"])
        && button_is_complete(room["BtCoin"])
        && button_is_complete(room["BtEnter"])
        && button_is_complete(room["BtReset"])
        && button_is_complete(room["BtTrade"]);
    std::cout << (ok ? "PASS" : "FAIL")
              << " CashTradingRoom 654x566 and required buttons\n";
    return ok ? 0 : 1;
}
