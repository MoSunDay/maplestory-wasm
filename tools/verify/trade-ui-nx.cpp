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

    bool origin_is(nl::node node, int16_t x, int16_t y)
    {
        const auto origin = node["origin"].get_vector();
        return origin.first == x && origin.second == y;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "UI.nx";
    nl::file ui(path);
    nl::node room = ui.root()["UIWindow2.img"]["CashTradingRoom"];
    bool ok = bitmap_is(room["backgrnd"], 654, 566)
        && bitmap_is(room["backgrnd2"], 641, 553)
        && bitmap_is(room["backgrnd3"], 632, 437)
        && origin_is(room["backgrnd"], 0, 1)
        && origin_is(room["backgrnd2"], -7, -6)
        && origin_is(room["backgrnd3"], -11, -118)
        && button_is_complete(room["BtClame"])
        && button_is_complete(room["BtCoin"])
        && button_is_complete(room["BtEnter"])
        && button_is_complete(room["BtReset"])
        && button_is_complete(room["BtTrade"])
        && origin_is(room["BtClame"]["normal"]["0"], -626, -363)
        && origin_is(room["BtCoin"]["normal"]["0"], -203, -348)
        && origin_is(room["BtEnter"]["normal"]["0"], -583, -364)
        && origin_is(room["BtReset"]["normal"]["0"], -168, -87)
        && origin_is(room["BtTrade"]["normal"]["0"], -168, -65);
    std::cout << (ok ? "PASS" : "FAIL")
              << " CashTradingRoom NX origins and required buttons\n";
    return ok ? 0 : 1;
}
