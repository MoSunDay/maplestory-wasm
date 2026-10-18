//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UIShop.h"
#include "UINotice.h"
#include "UIShopNodes.h"

#include "../UI.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Data/ItemData.h"
#include "../../Net/Packets/NpcInteractionPackets.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"


namespace jrc
{
    UIShop::UIShop(const CharLook& in_charlook, const Inventory& in_inventory)
        : charlook(in_charlook), inventory(in_inventory), last_cursor_pos()
    {
        nl::node src = shop_nodes::src();

        sprites.emplace_back(src["backgrnd"]);
        // v83 ships a single merged background layer.
        if (nl::node backgrnd2 = src["backgrnd2"])
        {
            sprites.emplace_back(backgrnd2);
            sprites.emplace_back(src["backgrnd3"]);
        }

        buttons[BUY_ITEM]  = std::make_unique<MapleButton>(src["BtBuy"], shop_nodes::buy_button_pos());
        buttons[SELL_ITEM] = std::make_unique<MapleButton>(src["BtSell"], shop_nodes::sell_button_pos());
        buttons[SELL_ALL]  = std::make_unique<MapleButton>(src["BtSell"], shop_nodes::sell_all_button_pos());
        buttons[EXIT]      = std::make_unique<MapleButton>(src["BtExit"], shop_nodes::exit_button_pos());
        if (nl::node rechargenode = src["BtRecharge"])
        {
            for (uint16_t i = RECHARGE0; i <= RECHARGE4; ++i)
            {
                auto recharge = std::make_unique<MapleButton>(
                    rechargenode,
                    shop_nodes::recharge_pos(static_cast<int16_t>(i - RECHARGE0))
                );
                recharge->set_active(false);
                buttons[i] = std::move(recharge);
            }
        }

        nl::node selltabs = shop_nodes::sell_tabs();
        nl::node sellen   = selltabs["enabled"];
        nl::node selldis  = selltabs["disabled"];

        for (uint16_t i = EQUIP; i <= CASH; ++i)
        {
            std::string tabnum = std::to_string(i - EQUIP);
            buttons[i] = std::make_unique<TwoSpriteButton>(
                selldis[tabnum],
                sellen[tabnum],
                shop_nodes::sell_tab_pos(static_cast<int16_t>(i - EQUIP))
            );
        }

        for (uint16_t i = BUY0; i <= BUY4; ++i)
        {
            Point<int16_t> pos = shop_nodes::buy_item_button_pos(static_cast<int16_t>(i - BUY0));
            Point<int16_t> dim(shop_nodes::row_width(), shop_nodes::row_height());
            buttons[i] = std::make_unique<AreaButton>(pos, dim);
        }
        for (uint16_t i = SELL0; i <= SELL4; ++i)
        {
            Point<int16_t> pos = shop_nodes::sell_item_button_pos(static_cast<int16_t>(i - SELL0));
            Point<int16_t> dim(shop_nodes::row_width(), shop_nodes::row_height());
            buttons[i] = std::make_unique<AreaButton>(pos, dim);
        }

        selection  = src["select"];
        impossible = src["ShopSpecial"]["impossible"];
        meso       = src["meso"];

        mesolabel = { Text::A11M, Text::RIGHT, Text::LIGHTGREY };

        const Point<int16_t> buysliderpos = shop_nodes::buy_slider_pos();
        const Point<int16_t> sellsliderpos = shop_nodes::sell_slider_pos();

        buyslider  = {
            11, { buysliderpos.y(), static_cast<int16_t>(buysliderpos.y() + shop_nodes::buy_slider_height()) }, buysliderpos.x(), 5, 1,
            [&](bool upwards) {
                int16_t shift = upwards ? -1 : 1;
                bool above = buystate.offset + shift >= 0;
                bool below = buystate.offset + shift <= buystate.lastslot - 5;
                if (above && below)
                {
                    buystate.offset += shift;
                }
            }
        };
        sellslider = {
            11, { sellsliderpos.y(), static_cast<int16_t>(sellsliderpos.y() + shop_nodes::sell_slider_height()) }, sellsliderpos.x(), 5, 1,
            [&](bool upwards) {
                int16_t shift = upwards ? -1 : 1;
                bool above = sellstate.offset + shift >= 0;
                bool below = sellstate.offset + shift <= sellstate.lastslot - 5;
                if (above && below)
                {
                    sellstate.offset += shift;
                }
            }
        };

        active    = false;
        dimension = Texture(src["backgrnd"]).get_dimensions();
        position  = { 400 - dimension.x() / 2, 240 - dimension.y() / 2 };
    }

    void UIShop::draw(float alpha) const
    {
        UIElement::draw(alpha);

        if (shop_nodes::has_portraits())
        {
            npc.draw({ position + Point<int16_t>(64, 76), true });
            charlook.draw(
                position + Point<int16_t>(294, 76),
                false,
                Stance::STAND1,
                Expression::DEFAULT
            );
        }

        mesolabel.draw(position + shop_nodes::meso_label_pos());
        buystate.draw(position, selection);
        sellstate.draw(position, selection);

        buyslider.draw(position);
        sellslider.draw(position);
    }

    void UIShop::update()
    {
        int64_t num_mesos   = inventory.get_meso();
        std::string mesostr = std::to_string(num_mesos);
        string_format::split_number(mesostr);
        mesolabel.change_text(mesostr);
    }

    Button::State UIShop::button_pressed(uint16_t buttonid)
    {
        clear_tooltip();

        constexpr Range<uint16_t> recharge(RECHARGE0, RECHARGE4);
        constexpr Range<uint16_t> buy(BUY0, BUY4);
        constexpr Range<uint16_t> sell(SELL0, SELL4);
        if (recharge.contains(buttonid))
        {
            int16_t selected = buttonid - RECHARGE0;
            sellstate.selection = selected + sellstate.offset;
            sellstate.recharge_at(selected);
            update_recharge_buttons();

            return Button::NORMAL;
        }
        else if (buy.contains(buttonid))
        {
            int16_t selected = buttonid - BUY0;
            buystate.select(selected);

            return Button::NORMAL;
        }
        else if(sell.contains(buttonid))
        {
            int16_t selected = buttonid - SELL0;
            sellstate.select(selected);
            update_recharge_buttons();

            return Button::NORMAL;
        }
        else
        {
            switch (buttonid)
            {
            case BUY_ITEM:
                buystate.buy();
                return Button::NORMAL;
            case SELL_ITEM:
                sellstate.sell();
                return Button::NORMAL;
            case SELL_ALL:
                if (!sellstate.has_sellable_items())
                {
                    return Button::DISABLED;
                }
                sellstate.sell_all();
                return Button::NORMAL;
            case EXIT:
                active = false;
                NpcShopActionPacket().dispatch();
                return Button::PRESSED;
            case EQUIP:
                changeselltab(InventoryType::EQUIP);
                return Button::IDENTITY;
            case USE:
                changeselltab(InventoryType::USE);
                return Button::IDENTITY;
            case ETC:
                changeselltab(InventoryType::ETC);
                return Button::IDENTITY;
            case SETUP:
                changeselltab(InventoryType::SETUP);
                return Button::IDENTITY;
            case CASH:
                changeselltab(InventoryType::CASH);
                return Button::IDENTITY;
            default:
                break;
            }
        }
        return Button::PRESSED;
    }

    bool UIShop::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (buyslider.remove_cursor(clicked))
        {
            return true;
        }

        if (sellslider.remove_cursor(clicked))
        {
            return true;
        }

        return UIElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIShop::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        Point<int16_t> cursoroffset = cursorpos - position;
        last_cursor_pos = cursoroffset;

        if (buyslider.isenabled())
        {
            Cursor::State bstate = buyslider.send_cursor(cursoroffset, clicked);
            if (bstate != Cursor::IDLE)
            {
                clear_tooltip();
                return { bstate, true };
            }
        }

        if (sellslider.isenabled())
        {
            Cursor::State sstate = sellslider.send_cursor(cursoroffset, clicked);
            if (sstate != Cursor::IDLE)
            {
                update_recharge_buttons();
                clear_tooltip();
                return { sstate, true };
            }
        }

        int16_t xoff = cursoroffset.x();
        int16_t yoff = cursoroffset.y();
        int16_t slot = slot_by_position(yoff);
        if (slot >= 0 && slot <= 4)
        {
            if (shop_nodes::cursor_over_buy_list(xoff))
            {
                show_item(slot, true);
            }
            else if (shop_nodes::cursor_over_sell_list(xoff))
            {
                show_item(slot, false);
            }
            else
            {
                clear_tooltip();
            }
        }
        else
        {
            clear_tooltip();
        }
        return UIElement::send_cursor(clicked, cursorpos);
    }

    void UIShop::send_scroll(double yoffset)
    {
        int16_t xoff = last_cursor_pos.x();
        int16_t yoff = last_cursor_pos.y();
        if (yoff < 115 || yoff > 308)
        {
            return;
        }

        if (buyslider.isenabled() && shop_nodes::scroll_over_buy_list(xoff))
        {
            buyslider.send_scroll(yoffset);
        }
        else if (sellslider.isenabled() && shop_nodes::scroll_over_sell_list(xoff))
        {
            sellslider.send_scroll(yoffset);
            update_recharge_buttons();
        }
    }

    void UIShop::rightclick(Point<int16_t> cursorpos)
    {
        Point<int16_t> cursoroffset = cursorpos - position;
        int16_t slot = slot_by_position(cursoroffset.y());
        if (slot < 0 || slot > 4)
        {
            return;
        }

        if (cursoroffset.x() > 241 && cursoroffset.x() < 443)
        {
            clear_tooltip();
            buystate.selection = -1;
            sellstate.selection = slot + sellstate.offset;
            update_recharge_buttons();
            sellstate.sell();
        }
    }

    void UIShop::send_key(int32_t, bool pressed, bool escape)
    {
        if (!pressed || !escape)
        {
            return;
        }

        active = false;
        NpcShopActionPacket().dispatch();
    }

    void UIShop::clear_tooltip()
    {
        UI::get().clear_tooltip(Tooltip::SHOP);
    }

    void UIShop::show_item(int16_t slot, bool buy)
    {
        if (buy)
        {
            buystate.show_item(slot);
        }
        else
        {
            sellstate.show_item(slot);
        }
    }

    void UIShop::changeselltab(InventoryType::Id type)
    {
        uint16_t oldtab = tabbyinventory(sellstate.tab);
        if (oldtab > 0)
        {
            buttons[oldtab]->set_state(Button::NORMAL);
        }

        uint16_t newtab = tabbyinventory(type);
        if (newtab > 0)
        {
            buttons[newtab]->set_state(Button::PRESSED);
        }

        sellstate.change_tab(inventory, type, meso);

        sellslider.setrows(5, sellstate.lastslot);
        update_recharge_buttons();
        update_sell_all_button();
    }

    void UIShop::update_recharge_buttons()
    {
        for (uint16_t i = RECHARGE0; i <= RECHARGE4; ++i)
        {
            auto iter = buttons.find(i);
            if (iter == buttons.end())
            {
                continue;
            }

            Button* recharge = iter->second.get();
            int16_t visibleslot = i - RECHARGE0;
            if (sellstate.can_recharge_at(visibleslot))
            {
                recharge->set_active(true);
                recharge->set_state(Button::NORMAL);
            }
            else
            {
                recharge->set_active(false);
                recharge->set_state(Button::NORMAL);
            }
        }
    }

    void UIShop::update_sell_all_button()
    {
        buttons[SELL_ALL]->set_state(
            sellstate.has_sellable_items() ? Button::NORMAL : Button::DISABLED
        );
    }

    void UIShop::reset(int32_t npcid)
    {
        std::string strid = string_format::extend_id(npcid, 7);
        npc = nl::nx::npc[strid + ".img"]["stand"]["0"];

        for (auto& button : buttons)
        {
            button.second->set_state(Button::NORMAL);
        }
        buttons[EQUIP]->set_state(Button::PRESSED);

        buystate.reset();
        sellstate.reset();

        changeselltab(InventoryType::EQUIP);

        active = true;
    }

    void UIShop::modify(InventoryType::Id type)
    {
        if (type == sellstate.tab)
        {
            changeselltab(type);
        }
    }

    int32_t UIShop::visible_buy_item(int16_t row) const
    {
        return buystate.visible_item(row);
    }

    Point<int16_t> UIShop::visible_buy_center(int16_t row) const
    {
        if (row < 0 || row >= 5)
            return { -32768, -32768 };
        return position + Point<int16_t>(108, static_cast<int16_t>(134 + 42 * row));
    }

    int32_t UIShop::visible_sell_item(int16_t row) const
    {
        const int16_t slot = row + sellstate.offset;
        if (row < 0 || row >= 5 || slot < 0 || slot >= sellstate.lastslot)
        {
            return 0;
        }
        return sellstate.items[slot].get_id();
    }

    Point<int16_t> UIShop::visible_sell_center(int16_t row) const
    {
        if (row < 0 || row >= 5)
        {
            return { -32768, -32768 };
        }
        return position + shop_nodes::sell_item_button_pos(row)
            + Point<int16_t>(
                static_cast<int16_t>(shop_nodes::row_width() / 2),
                static_cast<int16_t>(shop_nodes::row_height() / 2));
    }

    Point<int16_t> UIShop::button_center(uint16_t id) const
    {
        const auto iter = buttons.find(id);
        if (iter == buttons.end())
        {
            return { -32768, -32768 };
        }
        const Rectangle<int16_t> rect = iter->second->bounds(position);
        return {
            static_cast<int16_t>((rect.l() + rect.r()) / 2),
            static_cast<int16_t>((rect.t() + rect.b()) / 2)
        };
    }

    int16_t UIShop::sell_selection() const
    {
        return sellstate.selection;
    }

    void UIShop::add_item(int32_t id,
                          int32_t price,
                          int32_t pitch,
                          int32_t time,
                          int16_t buyable)
    {
        add_rechargable(id, price, pitch, time, 0, buyable);
    }

    void UIShop::add_rechargable(int32_t id,
                                 int32_t price,
                                 int32_t pitch,
                                 int32_t time,
                                 int16_t chargeprice,
                                 int16_t buyable)
    {
        auto buyitem = BuyItem(meso, id, price, pitch, time, chargeprice, buyable);
        buystate.add(buyitem);

        buyslider.setrows(5, buystate.lastslot);
    }

    int16_t UIShop::slot_by_position(int16_t y)
    {
        const int16_t yoff = static_cast<int16_t>(y - shop_nodes::row_top());
        if (yoff < 0)
        {
            return -1;
        }

        const int16_t slot = static_cast<int16_t>(yoff / shop_nodes::row_step());
        if (slot < 0 || slot > 4)
        {
            return -1;
        }

        const int16_t row_start = static_cast<int16_t>(slot * shop_nodes::row_step());
        if (yoff - row_start >= shop_nodes::row_height())
        {
            return -1;
        }

        return slot;
    }

    uint16_t UIShop::tabbyinventory(InventoryType::Id type)
    {
        switch (type)
        {
        case InventoryType::EQUIP:
            return EQUIP;
        case InventoryType::USE:
            return USE;
        case InventoryType::ETC:
            return ETC;
        case InventoryType::SETUP:
            return SETUP;
        case InventoryType::CASH:
            return CASH;
        default:
            return 0;
        }
    }


    UIShop::BuyItem::BuyItem(Texture cur,
                             int32_t i,
                             int32_t p,
                             int32_t pt,
                             int32_t t,
                             int16_t cp,
                             int16_t b)
        : currency(cur), id(i), price(p), pitch(pt), time(t), chargeprice(cp),
          buyable(b)
    {
        namelabel  = { Text::A11M, Text::LEFT, Text::DARKGREY };
        pricelabel = { Text::A11M, Text::LEFT, Text::DARKGREY };

        const ItemData& item = ItemData::get(id);
        if (item.is_valid())
        {
            icon = item.get_icon(false);
            namelabel.change_text(item.get_name());
        }

        std::string mesostr = std::to_string(price);
        string_format::split_number(mesostr);
        pricelabel.change_text(mesostr + " Mesos");
    }

    void UIShop::BuyItem::draw(Point<int16_t> pos) const
    {
        if (shop_nodes::post_chaos())
        {
            icon.draw(pos + Point<int16_t>(0, 32));
            namelabel.draw(pos + Point<int16_t>(40, -1));
            currency.draw(pos + Point<int16_t>(38, 20));
            pricelabel.draw(pos + Point<int16_t>(53, 17));
        }
        else
        {
            // v83 rows are 35px tall with the icon slot flush at the row
            // origin and origin-less item icons.
            icon.draw(pos + Point<int16_t>(2, 2));
            namelabel.draw(pos + Point<int16_t>(38, 1));
            currency.draw(pos + Point<int16_t>(36, 20));
            pricelabel.draw(pos + Point<int16_t>(51, 18));
        }
    }

    int32_t UIShop::BuyItem::get_id() const
    {
        return id;
    }

    int16_t UIShop::BuyItem::get_buyable() const
    {
        return buyable;
    }

    void UIShop::BuyState::reset()
    {
        items.clear();

        offset    = 0;
        lastslot  = 0;
        selection = -1;
    }

    void UIShop::BuyState::draw(Point<int16_t> parentpos, const Texture& selected) const
    {
        for (int16_t i = 0; i < 5; ++i)
        {
            int16_t slot = i + offset;
            if (slot >= lastslot)
            {
                break;
            }

            Point<int16_t> itempos = shop_nodes::buy_itempos(i);
            if (slot == selection)
            {
                selected.draw(parentpos + itempos + shop_nodes::selection_shift());
            }
            items[slot].draw(parentpos + itempos);
        }
    }

    void UIShop::BuyState::show_item(int16_t slot)
    {
        int16_t absslot = slot + offset;
        if (absslot < 0 || absslot >= lastslot)
        {
            return;
        }

        int32_t itemid = items[absslot].get_id();
        UI::get().show_item(Tooltip::SHOP, itemid);
    }

    void UIShop::BuyState::add(BuyItem item)
    {
        items.push_back(item);

        lastslot++;
    }

    void UIShop::BuyState::buy() const
    {
        if (selection < 0 || selection >= lastslot)
        {
            return;
        }

        const BuyItem& item = items[selection];
        int16_t buyable = item.get_buyable();
        int16_t slot = selection;
        int32_t itemid = item.get_id();
        if (buyable > 1)
        {
            constexpr auto question = "How many would you like to buy?";
            auto onenter = [slot, itemid](int32_t qty){
                auto shortqty = static_cast<int16_t>(qty);

                NpcShopActionPacket(slot, itemid, shortqty, true).dispatch();
            };
            UI::get().emplace<UIEnterNumber>(question, onenter, 1, buyable, 1);
        }
        else if (buyable > 0)
        {
            constexpr auto question = "Would you like to buy the item?";
            auto ondecide = [slot, itemid](bool yes){
                if (yes)
                {
                    NpcShopActionPacket(slot, itemid, 1, true).dispatch();
                }
            };
            UI::get().emplace<UIYesNo>(question, ondecide);
        }
    }

    void UIShop::BuyState::select(int16_t selected)
    {
        int16_t slot = selected + offset;
        if (slot == selection)
        {
            buy();
        }
        else
        {
            selection = slot;
        }
    }

    int32_t UIShop::BuyState::visible_item(int16_t row) const
    {
        int16_t slot = static_cast<int16_t>(row + offset);
        return row >= 0 && row < 5 && slot >= 0 && slot < lastslot
            ? items[slot].get_id()
            : 0;
    }


}
