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
#include "UIItemInventory.h"

#include "UIItemInventoryNodes.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "UIKeyConfig.h"
#include "../Trade/UITrade.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Net/Packets/PetPackets.h"
#include "../../Net/Packets/PlayerPackets.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    UIItemInventory::UIItemInventory(const Inventory& invent)
        : UIDragElement<PosINV>(iteminv_nodes::drag_position()), inventory(invent) {

        // The window ships as two different node sets: post-Chaos reads
        // UIWindow2.img/Item (172x335, layered backgrounds, origin
        // bearing buttons, extra panels), pre-Chaos v83 reads
        // UIWindow.img/Item (175x288, single baked background).
        // iteminv_nodes picks the generation matching the loaded UI.nx.
        nl::node src = iteminv_nodes::src();

        sprites.emplace_back(src["backgrnd"]);
        if (iteminv_nodes::has_layered_background())
        {
            // Post-Chaos paints the sockets and the tab strip/meso row on
            // their own backgrnd2/backgrnd3 layers.
            sprites.emplace_back(src["backgrnd2"]);
            sprites.emplace_back(src["backgrnd3"]);
        }

        newitemslot = src["New"]["inventory"];
        newitemtab = src["New"]["Tab0"];
        projectile = src["activeIcon"];

        nl::node taben = src["Tab"]["enabled"];
        nl::node tabdis = src["Tab"]["disabled"];

        // Post-Chaos tab sprites carry origins and only need the upstream
        // nudge; v83 sprites carry none, so the strip is laid out
        // explicitly (five 27px tabs, 32px apart, on the dark band
        // between the title bar and the slot grid).
        buttons[BT_TAB_EQUIP]= std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"], iteminv_nodes::tab_button_pos(0));
        buttons[BT_TAB_USE] = std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"], iteminv_nodes::tab_button_pos(1));
        buttons[BT_TAB_ETC] = std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"], iteminv_nodes::tab_button_pos(2));
        buttons[BT_TAB_SETUP] = std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"], iteminv_nodes::tab_button_pos(3));
        buttons[BT_TAB_CASH]= std::make_unique<TwoSpriteButton>(tabdis["4"], taben["4"], iteminv_nodes::tab_button_pos(4));

        // Post-Chaos button art carries origins, so the buttons position
        // themselves; v83 frames are centre-anchored (origin 6,6) or plain
        // (BtCoin), so each one is placed on its socket in the backgrnd:
        // the meso coin at (8, 267), the gather/sort pair in the dark
        // panel at the bottom right and the expand toggle in the title
        // bar.
        buttons[BT_DROPMESO]= std::make_unique<MapleButton>(src["BtCoin"], iteminv_nodes::drop_meso_pos());
        buttons[BT_GATHER]= std::make_unique<MapleButton>(src["BtGather"], iteminv_nodes::gather_pos());
        buttons[BT_SORT]= std::make_unique<MapleButton>(src["BtSort"], iteminv_nodes::sort_pos());
        buttons[BT_EXPAND]= std::make_unique<MapleButton>(src["BtFull"], iteminv_nodes::expand_pos());

        if (iteminv_nodes::has_extra_buttons())
        {
            // Post-Chaos-only chrome: the meso point wallet and the item
            // pot / upgrade / magnify / bit case panels. v83 does not ship
            // them and they simply stay absent.
            buttons[BT_POINTS]= std::make_unique<MapleButton>(src["BtPoint0"]);
            buttons[BT_ITEMPOT]= std::make_unique<MapleButton>(src["BtPot3"]);
            buttons[BT_UPGRADE]= std::make_unique<MapleButton>(src["BtUpgrade3"]);
            buttons[BT_MAGNIFY]= std::make_unique<MapleButton>(src["BtAppraise3"]);
            buttons[BT_BITCASE]= std::make_unique<MapleButton>(src["BtBits3"]);
        }

        tab = InventoryType::EQUIP;
        slotrange.first = 1;
        slotrange.second = 24;
        newtab = InventoryType::NONE;
        newslot = 0;

        buttons[BT_SORT]->set_active(false);
        buttons[button_by_tab(tab)]->set_state(Button::PRESSED);

        mesolabel = { Text::A11M, Text::RIGHT, Text::LIGHTGREY };

        // The scrollbar runs along the strip between the last socket
        // column and the border; its arrows span the six socket rows.
        slider = {
            11, iteminv_nodes::slider_vertical(), iteminv_nodes::slider_x(),
            6, 1 + inventory.get_slotmax(tab) / 4,
            [&](bool upwards) {
            int16_t shift = upwards ? -4 : 4;
            bool above = slotrange.first + shift > 0;
            bool below = slotrange.second + shift < inventory.get_slotmax(tab) + 1 + 4;
            if (above && below)
            {
                slotrange.first += shift;
                slotrange.second += shift;
            }
        } };

        dimension = iteminv_nodes::dimension();
        active = true;

        load_icons();
    }

    void UIItemInventory::draw(float alpha) const
    {
        UIElement::draw(alpha);

        slider.draw(position);

        for (auto& icon : icons)
        {
            int16_t slot = icon.first;
            if (icon.second && slot >= slotrange.first && slot <= slotrange.second)
            {
                Point<int16_t> slotpos = get_slotpos(slot);
                icon.second->draw(position + slotpos);
            }
        }

        int16_t bulletslot = inventory.get_bulletslot();
        if (tab == InventoryType::USE && is_visible(bulletslot))
        {
            Point<int16_t> bulletslotpos = position + get_slotpos(bulletslot);
            projectile.draw({ bulletslotpos });
        }
        else if (newtab == tab && is_visible(newslot))
        {
            Point<int16_t> newslotpos = position + get_slotpos(newslot);
            newslotpos.shift_y(1);
            newitemslot.draw({ newslotpos }, alpha);
        }

        if (newtab != tab && newtab != InventoryType::NONE)
        {
            Point<int16_t> newtabpos = position + get_tabpos(newtab);
            newitemtab.draw({ newtabpos }, alpha);
        }

        Point<int16_t> mesopos = position + iteminv_nodes::meso_label_pos();
        mesolabel.draw(mesopos);
    }

    void UIItemInventory::update()
    {
        UIElement::update();

        newitemtab.update(6);
        newitemslot.update(6);

        int64_t meso = inventory.get_meso();
        std::string mesostr = std::to_string(meso);
        string_format::split_number(mesostr);
        mesolabel.change_text(mesostr);
    }

    void UIItemInventory::update_slot(int16_t slot)
    {
        if (int32_t item_id = inventory.get_item_id(tab, slot))
        {
            int16_t count;
            if (tab == InventoryType::EQUIP)
            {
                count = -1;
            }
            else
            {
                count = inventory.get_item_count(tab, slot);
            }

            const Texture& texture = ItemData::get(item_id).get_icon(false);
            Equipslot::Id eqslot = inventory.find_equipslot(item_id);
            icons[slot] = std::make_unique<Icon>(
                std::make_unique<ItemIcon>(tab, eqslot, slot, item_id),
                texture,
                count
                );
        }
        else if (icons.count(slot))
        {
            icons.erase(slot);
        }
    }

    void UIItemInventory::load_icons()
    {
        icons.clear();

        int16_t numslots = inventory.get_slotmax(tab);
        for (int16_t i = 1; i <= numslots; ++i)
        {
            update_slot(i);
        }
    }

    Button::State UIItemInventory::button_pressed(uint16_t buttonid)
    {
        InventoryType::Id oldtab = tab;
        switch (buttonid)
        {
        case BT_TAB_EQUIP:
            tab = InventoryType::EQUIP;
            break;
        case BT_TAB_USE:
            tab = InventoryType::USE;
            break;
        case BT_TAB_SETUP:
            tab = InventoryType::SETUP;
            break;
        case BT_TAB_ETC:
            tab = InventoryType::ETC;
            break;
        case BT_TAB_CASH:
            tab = InventoryType::CASH;
            break;
        case BT_GATHER:
            GatherItemsPacket(tab).dispatch();
            break;
        case BT_SORT:
            SortItemsPacket(tab).dispatch();
            break;
        }

        if (tab != oldtab)
        {
            slotrange.first = 1;
            slotrange.second = 24;

            slider.setrows(6, 1 + inventory.get_slotmax(tab) / 4);

            buttons[button_by_tab(oldtab)]->set_state(Button::NORMAL);

            load_icons();
            enable_gather();
        }
        return Button::PRESSED;
    }

    void UIItemInventory::doubleclick(Point<int16_t> cursorpos)
    {
        int16_t slot = slot_by_position(cursorpos - position);
        if (icons.count(slot) && is_visible(slot))
        {
            if (int32_t item_id = inventory.get_item_id(tab, slot))
            {
                if (auto trade = UI::get().get_element<UITrade>();
                    trade && trade->is_active())
                {
                    trade->offer_inventory_item(tab, slot,
                        inventory.get_item_count(tab, slot));
                    return;
                }
                switch (tab)
                {
                case InventoryType::EQUIP:
                    EquipItemPacket(
                        slot,
                        inventory.find_equipslot(item_id)
                    ).dispatch();
                    break;
                case InventoryType::USE:
                    if (
                        item_id / 10000 != 204 &&
                        item_id / 10000 != 206 &&
                        item_id / 10000 != 207
                    ) {
                        UseItemPacket(slot, item_id).dispatch();
                    }
                    break;
                case InventoryType::SETUP:
                    Stage::get().get_player().use_item(item_id);
                    break;
                case InventoryType::CASH:
                    if (inventory.is_pet_item(tab, slot))
                    {
                        SpawnPetPacket(static_cast<int8_t>(slot)).dispatch();
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    void UIItemInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
    {
        int16_t slot = slot_by_position(cursorpos - position);
        if (slot > 0)
        {
            int32_t item_id = inventory.get_item_id(tab, slot);
            Equipslot::Id eqslot;
            bool equip;
            if (item_id && tab == InventoryType::EQUIP)
            {
                eqslot = inventory.find_equipslot(item_id);
                equip = true;
            }
            else
            {
                eqslot = Equipslot::NONE;
                equip = false;
            }
            icon.drop_on_items(tab, eqslot, slot, equip);
        }
    }

    UIElement::CursorResult UIItemInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            clear_tooltip();
            return UIDragElement::send_cursor(pressed, cursorpos);
        }

        Point<int16_t> cursor_relative = cursorpos - position;
        if (slider.isenabled())
        {
            Cursor::State sstate = slider.send_cursor(cursor_relative, pressed);
            if (sstate != Cursor::IDLE)
            {
                clear_tooltip();
                return { sstate, true };
            }
        }

        int16_t slot = slot_by_position(cursor_relative);
        Icon* icon = get_icon(slot);
        if (icon && is_visible(slot))
        {
            if (pressed)
            {
                Point<int16_t> slotpos = get_slotpos(slot);
                icon->start_drag(cursor_relative - slotpos);
                UI::get().drag_icon(icon);

                clear_tooltip();
                return { Cursor::GRABBING, true };
            }
            else
            {
                show_item(slot);
                return { Cursor::CANGRAB, true };
            }
        }

        clear_tooltip();
        return UIDragElement::send_cursor(pressed, cursorpos);
    }

    void UIItemInventory::modify(InventoryType::Id type, int16_t slot, int8_t mode, int16_t arg)
    {
        if (slot <= 0)
            return;

        if (type == tab)
        {
            switch (mode)
            {
            case Inventory::ADD:
                update_slot(slot);
                newtab = type;
                newslot = slot;
                break;
            case Inventory::CHANGECOUNT:
            case Inventory::ADDCOUNT:
                if (auto icon = get_icon(slot))
                    icon->set_count(arg);
                break;
            case Inventory::SWAP:
                if (arg != slot)
                {
                    update_slot(slot);
                    update_slot(arg);
                }
                break;
            case Inventory::REMOVE:
                update_slot(slot);
                break;
            }
        }

        switch (mode)
        {
        case Inventory::ADD:
        case Inventory::ADDCOUNT:
            newtab = type;
            newslot = slot;
            break;
        case Inventory::CHANGECOUNT:
        case Inventory::SWAP:
        case Inventory::REMOVE:
            if (newslot == slot && newtab == type)
            {
                newslot = 0;
                newtab = InventoryType::NONE;
            }
            break;
        }
    }

    void UIItemInventory::enable_sort()
    {
        buttons[BT_GATHER]->set_active(false);
        buttons[BT_SORT]->set_active(true);
        buttons[BT_SORT]->set_state(Button::NORMAL);
    }

    void UIItemInventory::enable_gather()
    {
        buttons[BT_SORT]->set_active(false);
        buttons[BT_GATHER]->set_active(true);
        buttons[BT_GATHER]->set_state(Button::NORMAL);
    }

    void UIItemInventory::toggle_active()
    {
        clear_tooltip();
        UIElement::toggle_active();
    }

    bool UIItemInventory::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (UIDragElement::remove_cursor(clicked, cursorpos))
            return true;

        return slider.remove_cursor(clicked);
    }

    void UIItemInventory::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            toggle_active();
        }
    }

    void UIItemInventory::show_item(int16_t slot)
    {
        if (tab == InventoryType::EQUIP)
        {
            UI::get().show_equip(Tooltip::ITEMINVENTORY, slot);
        }
        else
        {
            int32_t item_id = inventory.get_item_id(tab, slot);
            UI::get().show_item(Tooltip::ITEMINVENTORY, item_id);
        }
    }

    void UIItemInventory::clear_tooltip()
    {
        UI::get().clear_tooltip(Tooltip::ITEMINVENTORY);
    }

    bool UIItemInventory::is_visible(int16_t slot) const
    {
        return !is_not_visible(slot);
    }

    bool UIItemInventory::is_not_visible(int16_t slot) const
    {
        return slot < slotrange.first || slot > slotrange.second;
    }

    int16_t UIItemInventory::slot_by_position(Point<int16_t> cursorpos) const
    {
        // Socket hit test against the generation's grid geometry (see
        // iteminv_nodes).
        const int16_t offset = iteminv_nodes::page_offset_at(cursorpos);
        if (offset < 0)
            return 0;

        const int16_t slot = slotrange.first + offset;
        return is_visible(slot) ? slot : 0;
    }

    Point<int16_t> UIItemInventory::get_slotpos(int16_t slot) const
    {
        return iteminv_nodes::slotpos(slot - slotrange.first);
    }

    Point<int16_t> UIItemInventory::get_tabpos(InventoryType::Id tb) const
    {
        // The "new item" flash position differs per generation; the
        // window itself shares the tab button strip.
        return iteminv_nodes::tab_flash_pos(tb);
    }

    uint16_t UIItemInventory::button_by_tab(InventoryType::Id tb) const
    {
        switch (tb)
        {
        case InventoryType::EQUIP:
            return BT_TAB_EQUIP;
        case InventoryType::USE:
            return BT_TAB_USE;
        case InventoryType::SETUP:
            return BT_TAB_SETUP;
        case InventoryType::ETC:
            return BT_TAB_ETC;
        default:
            return BT_TAB_CASH;
        }
    }

    Icon* UIItemInventory::get_icon(int16_t slot)
    {
        auto iter = icons.find(slot);
        if (iter != icons.end())
        {
            return iter->second.get();
        }
        else
        {
            return nullptr;
        }
    }


    UIItemInventory::ItemIcon::ItemIcon(InventoryType::Id st, Equipslot::Id eqs, int16_t s, int32_t id)
    {
        sourcetab = st;
        eqsource = eqs;
        source = s;
        item_id = id;
    }

    void UIItemInventory::ItemIcon::drop_on_stage() const
    {
        MoveItemPacket(sourcetab, source, 0, 1).dispatch();
    }

    void UIItemInventory::ItemIcon::drop_on_equips(Equipslot::Id eqslot) const
    {
        switch (sourcetab)
        {
        case InventoryType::EQUIP:
            if (eqsource == eqslot)
            {
                EquipItemPacket(source, eqslot).dispatch();
            }
            break;
        case InventoryType::USE:
            ScrollEquipPacket(source, eqslot).dispatch();
            break;
        default:
            break;
        }
    }

    void UIItemInventory::ItemIcon::drop_on_items(InventoryType::Id tab, Equipslot::Id, int16_t slot, bool) const
    {
        if (tab != sourcetab || slot == source)
            return;

        MoveItemPacket(tab, source, slot, 1).dispatch();
    }

    void UIItemInventory::ItemIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
    {
        if (sourcetab != InventoryType::USE && sourcetab != InventoryType::SETUP)
            return;

        if (auto keyconfig = UI::get().get_element<UIKeyConfig>())
        {
            Keyboard::Mapping mapping{ KeyType::ITEM, item_id };
            if (remove)
                keyconfig->unstage_mapping(mapping);
            else
                keyconfig->stage_mapping(cursorposition, mapping);
        }
    }

    bool UIItemInventory::ItemIcon::drop_on_pet_potion(bool hp) const
    {
        if (sourcetab != InventoryType::USE)
            return false;

        const ItemData& item = ItemData::get(item_id);
        if (!item || (hp ? !item.restores_hp() : !item.restores_mp()))
            return false;

        int32_t mode = hp ? 1 : 2;
        ChangeKeyMapPacket(mode, item_id).dispatch();
        if (hp)
            Stage::get().set_pet_auto_hp_item(item_id);
        else
            Stage::get().set_pet_auto_mp_item(item_id);
        return true;
    }

    void UIItemInventory::ItemIcon::drop_on_trade() const
    {
        if (auto trade = UI::get().get_element<UITrade>(); trade && trade->is_active())
        {
            const Inventory& inventory = Stage::get().get_player().get_inventory();
            trade->offer_inventory_item(sourcetab, source,
                inventory.get_item_count(sourcetab, source));
        }
    }
}
