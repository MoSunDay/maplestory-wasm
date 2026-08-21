#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
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
        return image.width() > 0 && image.height() > 0 &&
            (width == 0 || image.width() == width) &&
            (height == 0 || image.height() == height);
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
    const std::string ui_path = argc > 1 ? argv[1] : "UI.nx";
    const std::string catalog_path = argc > 2 ? argv[2] :
        "src/client/data/cash-shop-v83.csv";
    nl::file ui(ui_path);
    nl::node shop = ui.root()["UIWindow2.img"]["Shop"];

    bool ok = bitmap_is(shop["backgrnd"], 465, 328) &&
        bitmap_is(shop["backgrnd2"]) && bitmap_is(shop["backgrnd3"]) &&
        bitmap_is(shop["select"]) && button_is_complete(shop["BtBuy"]) &&
        button_is_complete(shop["BtExit"]);

    std::ifstream catalog_file(catalog_path, std::ios::binary);
    const std::string catalog{
        std::istreambuf_iterator<char>(catalog_file), std::istreambuf_iterator<char>()};
    const bool commodity_ok = catalog.find("C,80000002,4031192,1,0,1,-1,0,1") !=
        std::string::npos;
    const bool non_package = catalog.find("P,4031192,") == std::string::npos;
    ok = ok && commodity_ok && non_package;

    if (!ok)
        std::cerr << "FAIL classic Shop assets or linked-server commodity contract\n";
    else
    {
        nl::node cash_button = ui.root()["StatusBar2.img"]["mainBar"]
            ["BtCashShop"]["normal"]["0"];
        const auto origin = cash_button["origin"].get_vector();
        const auto image = cash_button.get_bitmap();
        nl::node ok_button = ui.root()["Basic.img"]["BtOK4"]["normal"]["0"];
        const auto ok_origin = ok_button["origin"].get_vector();
        const auto ok_image = ok_button.get_bitmap();
        nl::node notice = ui.root()["Basic.img"]["Notice6"];
        const auto top_image = notice["t"].get_bitmap();
        const auto center_image = notice["c_box"].get_bitmap();
        const auto box_image = notice["box"].get_bitmap();
        const auto bottom_image = notice["s"].get_bitmap();
        std::cout << "PASS classic Shop 465x328, buttons, SN 80000002; cash-button="
                  << origin.first << ',' << origin.second << ' '
                  << image.width() << 'x' << image.height() << "; ok-button="
                  << ok_origin.first << ',' << ok_origin.second << ' '
                  << ok_image.width() << 'x' << ok_image.height() << "; notice-parts="
                  << top_image.width() << 'x' << top_image.height() << ','
                  << center_image.height() << ',' << box_image.height() << ','
                  << bottom_image.height() << '\n';
    }
    return ok ? 0 : 1;
}
