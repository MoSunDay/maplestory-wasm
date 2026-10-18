#include "CashShopSkin.h"

#include "CashShopView.h"

#include "nlnx/nx.hpp"

#include <stdexcept>
#include <string>

namespace jrc::cash_shop_skin
{
    namespace
    {
        const char* const BUTTON_STATES[4] = {
            "normal", "pressed", "mouseOver", "disabled"
        };

        std::string describe(const nl::bitmap& image)
        {
            return std::to_string(image.width()) + "x" +
                std::to_string(image.height());
        }

        // Resolves a node by path and requires a bitmap of the exact size.
        nl::node require_bitmap(
            nl::node root, const std::string& path,
            uint16_t width, uint16_t height)
        {
            nl::node node = root.resolve(path);
            if (!node || node.data_type() != nl::node::type::bitmap)
                throw std::runtime_error("CashShop.img missing bitmap: " + path);
            const nl::bitmap image = node.get_bitmap();
            if (image.width() != width || image.height() != height)
                throw std::runtime_error(
                    "CashShop.img bitmap size mismatch: " + path + " expected " +
                    std::to_string(width) + "x" + std::to_string(height) +
                    " got " + describe(image));
            return node;
        }

        // Number glyphs vary between 5 and 8 pixels in width but share the
        // 11 pixel baseline of Base/Number.
        nl::node require_number(nl::node root, const std::string& path)
        {
            nl::node node = root.resolve(path);
            if (!node || node.data_type() != nl::node::type::bitmap)
                throw std::runtime_error("CashShop.img missing bitmap: " + path);
            const nl::bitmap image = node.get_bitmap();
            if (image.height() != 11 || image.width() < 5 || image.width() > 8)
                throw std::runtime_error(
                    "CashShop.img number glyph mismatch: " + path + " got " +
                    describe(image));
            return node;
        }

        // A button state keeps its bitmap in a numbered frame child.
        nl::node state_frame(nl::node state)
        {
            nl::node frame = state["0"];
            return frame ? frame : state;
        }

        nl::node button_state(nl::node src, size_t state)
        {
            nl::node named = src[BUTTON_STATES[state]];
            if (named)
                return state_frame(named);
            return state_frame(src[static_cast<int>(state)]);
        }

        // Requires all four button states as bitmaps of the exact size.
        nl::node require_button(
            nl::node root, const std::string& path,
            uint16_t width, uint16_t height)
        {
            nl::node src = root.resolve(path);
            if (!src)
                throw std::runtime_error("CashShop.img missing node: " + path);
            for (size_t state = 0; state < 4; ++state)
            {
                nl::node frame = button_state(src, state);
                if (!frame || frame.data_type() != nl::node::type::bitmap)
                    throw std::runtime_error(
                        "CashShop.img button state missing: " + path + "/" +
                        BUTTON_STATES[state]);
                const nl::bitmap image = frame.get_bitmap();
                if (image.width() != width || image.height() != height)
                    throw std::runtime_error(
                        "CashShop.img button size mismatch: " + path + "/" +
                        BUTTON_STATES[state] + " expected " +
                        std::to_string(width) + "x" + std::to_string(height) +
                        " got " + describe(image));
            }
            return src;
        }

        size_t glyph_index(char character)
        {
            return character == ',' ? COMMA_GLYPH :
                static_cast<size_t>(character - '0');
        }
    }

    CashShopSkin load_cash_shop_skin()
    {
        nl::node root = nl::nx::ui["CashShop.img"];
        CashShopSkin skin;
        skin.background = require_bitmap(root, "Base/backgrnd", 1024, 768);
        for (size_t digit = 0; digit < COMMA_GLYPH; ++digit)
            skin.number[digit] =
                require_number(root, "Base/Number/" + std::to_string(digit));
        skin.number[COMMA_GLYPH] = require_number(root, "Base/Number/,");
        skin.no_item = require_bitmap(root, "Base/noItem", 348, 148);
        skin.line = require_bitmap(root, "Base/line", 859, 2);
        skin.sort_bar = require_bitmap(root, "CSSortBar/backgrnd", 491, 32);
        skin.sort_top_button = require_button(root, "CSSortBar/BtTop", 42, 16);
        skin.sort_end_button = require_button(root, "CSSortBar/BtEnd", 42, 16);
        skin.card = require_bitmap(root, "CSList/Base", 119, 184);
        skin.buy_button = require_button(root, "CSList/BtBuy", 101, 24);
        skin.inventory_panel =
            require_bitmap(root, "CSInventory/backgrnd", 380, 213);
        skin.inventory_cover =
            require_bitmap(root, "CSInventory/backgrndCover", 346, 146);
        skin.tab_enabled =
            require_bitmap(root, "CSInventory/Tab/Enable/0", 70, 18);
        skin.tab_disabled =
            require_bitmap(root, "CSInventory/Tab/Disable/0", 70, 18);
        skin.tab_bar = require_bitmap(root, "CSInventory/Tab/bar", 362, 3);
        skin.transfer_button = require_button(
            root, "CSLockerNew/LockerTransfer/BtTransfer", 73, 16);
        skin.charge_button =
            require_button(root, "CSStatus/BtCharge", 77, 23);
        skin.exit_button = require_button(root, "CSTab/BtExit", 116, 35);
        skin.prev_button = require_button(root, "CSBargainSale/BtLeft", 30, 47);
        skin.next_button =
            require_button(root, "CSBargainSale/BtRight", 30, 47);
        skin.nx_icon = require_bitmap(root, "CSIcon/0", 15, 14);
        skin.selected_marker = require_bitmap(root, "CashItem/1", 12, 12);
        return skin;
    }

    int16_t amount_width(const CashShopSkin& skin, int32_t amount)
    {
        const std::string text = cash_shop_view::format_amount(amount);
        int16_t width = 0;
        for (char character : text)
            width += skin.number[glyph_index(character)].width();
        return width;
    }

    void draw_amount(
        const CashShopSkin& skin, Point<int16_t> position, int32_t amount)
    {
        const std::string text = cash_shop_view::format_amount(amount);
        int16_t x = position.x();
        for (char character : text)
        {
            const Texture& glyph = skin.number[glyph_index(character)];
            glyph.draw({ Point<int16_t>{ x, position.y() } });
            x += glyph.width();
        }
    }

    void draw_amount_right(
        const CashShopSkin& skin, Point<int16_t> right, int32_t amount)
    {
        draw_amount(
            skin,
            { static_cast<int16_t>(right.x() - amount_width(skin, amount)),
                right.y() },
            amount);
    }
}
