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
        return image.width() > 0 && image.height() > 0 &&
            (width == 0 || image.width() == width) &&
            (height == 0 || image.height() == height);
    }

    bool button_is_complete(nl::node button)
    {
        const char* states[] = { "normal", "pressed", "disabled", "mouseOver" };
        for (const char* state : states)
        {
            if (!bitmap_is(button[state]["0"]))
                return false;
        }
        return true;
    }

    bool button_fits(nl::node button, int16_t width, int16_t height)
    {
        const nl::node normal = button["normal"]["0"];
        if (!bitmap_is(normal))
            return false;
        const auto origin = normal["origin"].get_vector();
        const auto image = normal.get_bitmap();
        const int32_t left = -origin.first;
        const int32_t top = -origin.second;
        return left >= 0 && top >= 0 &&
            left + image.width() <= width && top + image.height() <= height;
    }

    bool bitmap_is_at(nl::node node, uint16_t width, uint16_t height,
        int16_t x, int16_t y)
    {
        if (!bitmap_is(node, width, height))
            return false;
        const auto origin = node["origin"].get_vector();
        return -origin.first == x && -origin.second == y;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "UI.nx";
    nl::file ui(path);
    const nl::node source = ui.root()["UIWindow2.img"]["UserInfo"];
    const nl::node character = source["character"];
    const nl::node pet = source["pet"];
    bool ok = true;
    const auto check = [&ok](bool condition, const std::string& label) {
        if (!condition)
            std::cerr << "FAIL missing UserInfo asset: " << label << '\n';
        ok = ok && condition;
    };
    check(bitmap_is(character["backgrnd"], 271, 190), "character/backgrnd");
    check(bitmap_is(character["backgrnd2"]), "character/backgrnd2");
    check(bitmap_is_at(character["name"], 89, 19, 14, 134),
        "character/name placement");
    check(bitmap_is_at(character["married"], 20, 17, 15, 113),
        "character/married placement");
    check(button_is_complete(character["BtPet"]), "character/BtPet");
    check(button_is_complete(character["BtTrad"]), "character/BtTrad");
    check(button_is_complete(character["BtParty"]), "character/BtParty");
    check(button_is_complete(character["BtPopUp"]), "character/BtPopUp");
    check(button_is_complete(character["BtPopDown"]), "character/BtPopDown");
    check(button_fits(character["BtPet"], 271, 190), "character/BtPet bounds");
    check(button_fits(character["BtTrad"], 271, 190), "character/BtTrad bounds");
    check(button_fits(character["BtParty"], 271, 190), "character/BtParty bounds");
    check(button_fits(character["BtPopUp"], 271, 190), "character/BtPopUp bounds");
    check(button_fits(character["BtPopDown"], 271, 190), "character/BtPopDown bounds");
    check(bitmap_is(pet["backgrnd"], 271, 196), "pet/backgrnd");
    check(bitmap_is(pet["backgrnd2"]), "pet/backgrnd2");
    check(bitmap_is(pet["backgrnd3"]), "pet/backgrnd3");
    for (int index = 0; index < 3; ++index)
    {
        const std::string key = std::to_string(index);
        const int16_t tab_x = static_cast<int16_t>(10 + index * 84);
        check(bitmap_is_at(pet["Tab"]["enabled"][key], 83, 13, tab_x, 11),
            "pet/Tab/enabled/" + key);
        check(bitmap_is_at(pet["Tab"]["disabled"][key], 83, 13, tab_x, 11),
            "pet/Tab/disabled/" + key);
        check(bitmap_is_at(pet["ActionPet"][key]["0"],
            index == 2 ? 240 : 238, 156, 11, 29),
            "pet/ActionPet/" + key);
    }
    std::cout << (ok ? "PASS" : "FAIL")
              << " UserInfo character, fame, party, trade, and three-pet assets";
    if (ok)
    {
        const nl::node pet_button = character["BtPet"]["normal"]["0"];
        const auto origin = pet_button["origin"].get_vector();
        const auto image = pet_button.get_bitmap();
        std::cout << "; pet-button=" << origin.first << ',' << origin.second
                  << ' ' << image.width() << 'x' << image.height();
    }
    std::cout << '\n';
    return ok ? 0 : 1;
}
