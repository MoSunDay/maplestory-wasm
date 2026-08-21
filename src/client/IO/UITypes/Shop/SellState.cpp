#include "../UIShop.h"
#include "../UINotice.h"
#include "SalePolicy.h"

#include "../../UI.h"
#include "../../Components/Charset.h"

#include "../../../Data/ItemData.h"
#include "../../../Net/Packets/NpcInteractionPackets.h"
#include "../../../Util/Misc.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    UIShop::SellItem::SellItem(int32_t item_id, int16_t count, int16_t s,
                               bool sc, Texture cur)
    {
        const ItemData& idata = ItemData::get(item_id);

        icon = idata.get_icon(false);
        id = item_id;
        sellable = count;
        slot = s;
        rechargable = item_id / 10000 == 207 || item_id / 10000 == 233;
        showcount = sc;
        currency = cur;

        namelabel = { Text::A11M, Text::LEFT, Text::DARKGREY };
        pricelabel = { Text::A11M, Text::LEFT, Text::DARKGREY };
        namelabel.change_text(idata.get_name());

        std::string mesostr = std::to_string(idata.get_price());
        string_format::split_number(mesostr);
        pricelabel.change_text(mesostr + " Mesos");
    }

    void UIShop::SellItem::draw(Point<int16_t> pos) const
    {
        icon.draw(pos + Point<int16_t>(0, 32));
        if (showcount)
        {
            static const Charset countset = {
                nl::nx::ui["Basic.img"]["ItemNo"], Charset::LEFT
            };
            countset.draw(std::to_string(sellable), pos + Point<int16_t>(0, 20));
        }
        namelabel.draw(pos + Point<int16_t>(40, -1));
        currency.draw(pos + Point<int16_t>(38, 20));
        pricelabel.draw(pos + Point<int16_t>(53, 17));
    }

    int32_t UIShop::SellItem::get_id() const
    {
        return id;
    }

    int16_t UIShop::SellItem::get_slot() const
    {
        return slot;
    }

    int16_t UIShop::SellItem::get_sellable() const
    {
        return sellable;
    }

    bool UIShop::SellItem::is_rechargable() const
    {
        return rechargable;
    }

    void UIShop::SellState::reset()
    {
        items.clear();
        offset = 0;
        lastslot = 0;
        selection = -1;
        tab = InventoryType::NONE;
    }

    void UIShop::SellState::change_tab(const Inventory& inventory,
                                       InventoryType::Id newtab, Texture meso)
    {
        tab = newtab;
        offset = 0;
        selection = -1;
        items.clear();

        int16_t slots = inventory.get_slotmax(tab);
        for (int16_t slot = 1; slot <= slots; ++slot)
        {
            int32_t item_id = inventory.get_item_id(tab, slot);
            bool cash = inventory.is_cash_item(tab, slot);
            if (item_id && shop_sale_policy::is_visible_sale_item(tab, cash))
            {
                int16_t count = inventory.get_item_count(tab, slot);
                items.emplace_back(item_id, count, slot,
                    tab != InventoryType::EQUIP, meso);
            }
        }

        lastslot = static_cast<int16_t>(items.size());
    }

    void UIShop::SellState::draw(Point<int16_t> parentpos,
                                 const Texture& selected) const
    {
        for (int16_t visible_slot = 0; visible_slot < 5; ++visible_slot)
        {
            int16_t slot = visible_slot + offset;
            if (slot >= lastslot)
            {
                break;
            }

            Point<int16_t> itempos(243, 116 + 42 * visible_slot);
            if (slot == selection)
            {
                selected.draw(parentpos + itempos + Point<int16_t>(35, -1));
            }
            items[slot].draw(parentpos + itempos);
        }
    }

    void UIShop::SellState::show_item(int16_t slot)
    {
        int16_t absslot = slot + offset;
        if (absslot < 0 || absslot >= lastslot)
        {
            return;
        }

        if (tab == InventoryType::EQUIP)
        {
            UI::get().show_equip(Tooltip::SHOP, items[absslot].get_slot());
        }
        else
        {
            UI::get().show_item(Tooltip::SHOP, items[absslot].get_id());
        }
    }

    bool UIShop::SellState::can_recharge_at(int16_t visibleslot) const
    {
        if (tab != InventoryType::USE)
        {
            return false;
        }

        int16_t absslot = visibleslot + offset;
        return absslot >= 0 && absslot < lastslot &&
            items[absslot].is_rechargable();
    }

    void UIShop::SellState::recharge_at(int16_t visibleslot) const
    {
        int16_t absslot = visibleslot + offset;
        if (can_recharge_at(visibleslot))
        {
            NpcShopActionPacket(items[absslot].get_slot()).dispatch();
        }
    }

    void UIShop::SellState::sell() const
    {
        if (selection < 0 || selection >= lastslot)
        {
            return;
        }

        const SellItem& item = items[selection];
        int32_t itemid = item.get_id();
        int16_t sellable = item.get_sellable();
        int16_t slot = item.get_slot();
        if (sellable > 1)
        {
            constexpr auto question = "How many would you like to sell?";
            auto onenter = [itemid, slot](int32_t qty) {
                NpcShopActionPacket(slot, itemid, static_cast<int16_t>(qty), false).dispatch();
            };
            UI::get().emplace<UIEnterNumber>(
                question, onenter, 1, sellable, 1, true
            );
        }
        else if (sellable > 0)
        {
            constexpr auto question = "Would you like to sell the item?";
            auto ondecide = [itemid, slot](bool yes) {
                if (yes)
                {
                    NpcShopActionPacket(slot, itemid, 1, false).dispatch();
                }
            };
            UI::get().emplace<UIYesNo>(question, ondecide);
        }
    }

    void UIShop::SellState::sell_all() const
    {
        if (!has_sellable_items())
        {
            return;
        }

        InventoryType::Id selected_tab = tab;
        auto ondecide = [selected_tab](bool yes) {
            if (yes)
            {
                NpcShopActionPacket::sell_all(selected_tab).dispatch();
            }
        };
        UI::get().emplace<UIYesNo>(
            "确定售卖当前分类中的全部非现金物品吗？", ondecide
        );
    }

    bool UIShop::SellState::has_sellable_items() const
    {
        return shop_sale_policy::supports_bulk_sale(tab) && !items.empty();
    }

    void UIShop::SellState::select(int16_t selected)
    {
        int16_t slot = selected + offset;
        if (slot == selection)
        {
            sell();
        }
        else
        {
            selection = slot;
        }
    }
}
