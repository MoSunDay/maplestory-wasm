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
    bool bitmap_is(nl::node node, uint16_t width, uint16_t height)
    {
        if (node.data_type() != nl::node::type::bitmap)
            return false;
        const auto image = node.get_bitmap();
        return image.width() == width && image.height() == height;
    }

    bool bitmap_is(nl::node node)
    {
        if (node.data_type() != nl::node::type::bitmap)
            return false;
        const auto image = node.get_bitmap();
        return image.width() > 0 && image.height() > 0;
    }

    bool button_is_complete(nl::node button, uint16_t width, uint16_t height)
    {
        const char* states[] = { "normal", "pressed", "disabled", "mouseOver" };
        for (const char* state : states)
            if (!bitmap_is(button[state]["0"], width, height))
                return false;
        return true;
    }

    // Number glyphs vary between 5 and 8 pixels wide on an 11px baseline.
    bool number_is(nl::node node)
    {
        if (node.data_type() != nl::node::type::bitmap)
            return false;
        const auto image = node.get_bitmap();
        return image.height() == 11 && image.width() >= 5 && image.width() <= 8;
    }

    bool catalog_assets_ok(nl::node shop)
    {
        bool ok = bitmap_is(shop["Base"]["backgrnd"], 1024, 768) &&
            bitmap_is(shop["Base"]["noItem"], 348, 148) &&
            bitmap_is(shop["Base"]["line"], 859, 2) &&
            bitmap_is(shop["CSSortBar"]["backgrnd"], 491, 32) &&
            button_is_complete(shop["CSSortBar"]["BtTop"], 42, 16) &&
            button_is_complete(shop["CSSortBar"]["BtEnd"], 42, 16) &&
            bitmap_is(shop["CSList"]["Base"], 119, 184) &&
            button_is_complete(shop["CSList"]["BtBuy"], 101, 24);
        for (uint16_t digit = 0; digit < 10; ++digit)
            ok = ok && number_is(shop["Base"]["Number"][std::to_string(digit)]);
        ok = ok && number_is(shop["Base"]["Number"][","]);
        return ok;
    }

    bool inventory_assets_ok(nl::node shop)
    {
        return bitmap_is(shop["CSInventory"]["backgrnd"], 380, 213) &&
            bitmap_is(shop["CSInventory"]["backgrndCover"], 346, 146) &&
            bitmap_is(shop["CSInventory"]["Tab"]["Enable"]["0"], 70, 18) &&
            bitmap_is(shop["CSInventory"]["Tab"]["Disable"]["0"], 70, 18) &&
            bitmap_is(shop["CSInventory"]["Tab"]["bar"], 362, 3) &&
            button_is_complete(
                shop["CSLockerNew"]["LockerTransfer"]["BtTransfer"], 73, 16);
    }

    bool status_assets_ok(nl::node shop)
    {
        return button_is_complete(shop["CSStatus"]["BtCharge"], 77, 23) &&
            button_is_complete(shop["CSTab"]["BtExit"], 116, 35) &&
            button_is_complete(shop["CSBargainSale"]["BtLeft"], 30, 47) &&
            button_is_complete(shop["CSBargainSale"]["BtRight"], 30, 47) &&
            bitmap_is(shop["CSIcon"]["0"], 15, 14) &&
            bitmap_is(shop["CashItem"]["1"], 12, 12);
    }
}

int main(int argc, char** argv)
{
    const std::string ui_path = argc > 1 ? argv[1] : "UI.nx";
    const std::string catalog_path = argc > 2 ? argv[2] :
        "src/client/data/cash-shop-v83.csv";
    nl::file ui(ui_path);
    nl::node shop = ui.root()["CashShop.img"];

    bool ok = catalog_assets_ok(shop) && inventory_assets_ok(shop) &&
        status_assets_ok(shop);

    std::ifstream catalog_file(catalog_path, std::ios::binary);
    const std::string catalog{
        std::istreambuf_iterator<char>(catalog_file), std::istreambuf_iterator<char>()};
    const bool commodity_ok = catalog.find("C,80000002,4031192,1,0,1,-1,0,1") !=
        std::string::npos;
    const bool non_package = catalog.find("P,4031192,") == std::string::npos;
    ok = ok && commodity_ok && non_package;

    if (!ok)
        std::cerr << "FAIL modern CashShop.img assets or linked-server commodity contract\n";
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
        std::cout << "PASS modern CashShop.img 1024x768 full-screen set, buttons, "
                  << "SN 80000002; cash-button="
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
