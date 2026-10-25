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
#include "UIStorage.h"
#include "UINotice.h"
#include "UIStorageNodes.h"

#include "../UILayout.h"
#include "../UI.h"
#include "../Components/AreaButton.h"
#include "../Components/Charset.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Constants.h"
#include "../../Character/Inventory/InventoryType.h"
#include "../../Data/ItemData.h"
#include "../../Net/Packets/NpcInteractionPackets.h"
#include "../../Graphics/Geometry.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace jrc
{
    namespace
    {

        constexpr InventoryType::Id INVENTORY_TABS[] = {
            InventoryType::EQUIP,
            InventoryType::USE,
            InventoryType::ETC,
            InventoryType::SETUP,
            InventoryType::CASH
        };

        int16_t max_offset(size_t size)
        {
            if (size <= storage_nodes::cells_visible())
            {
                return 0;
            }

            return static_cast<int16_t>(size - storage_nodes::cells_visible());
        }

        // Translucent overlay on the selected post-Chaos socket; the
        // legacy list uses the `select` row art underneath the icon.
        const ColorBox& cell_highlight()
        {
            static const ColorBox box = ColorBox(
                storage_nodes::GRID_CELL_SIZE,
                storage_nodes::GRID_CELL_SIZE,
                Geometry::WHITE,
                0.6f
            );
            return box;
        }

        // Draws one socket panel. The legacy list renders the selection
        // art beneath the icon inside the row backdrop and anchors the
        // icon on the slot inside it; the post-Chaos grid anchors the
        // icon on the cell corner and covers the selected cell with a
        // translucent ColorBox.
        template <typename EntryT>
        void draw_panel(bool inventory_side,
                        Point<int16_t> position,
                        const std::vector<EntryT>& items,
                        int16_t offset,
                        int16_t selection,
                        InventoryType::Id tab,
                        const Texture& selection_art,
                        const Texture& disabled_slot,
                        const Charset& countset)
        {
            const bool grid = storage_nodes::post_chaos();

            for (int16_t i = 0; i < storage_nodes::cells_visible(); ++i)
            {
                const int16_t index = static_cast<int16_t>(i + offset);
                const Point<int16_t> cellpos = position + storage_nodes::cell_pos(inventory_side, i);
                const Point<int16_t> iconpos = grid
                    ? cellpos
                    : position + storage_nodes::list_icon_pos(inventory_side, i);

                if (!grid && index == selection)
                {
                    selection_art.draw(cellpos + Point<int16_t>(storage_nodes::selection_shift(), 0));
                }

                if (index >= 0 && index < static_cast<int16_t>(items.size()))
                {
                    const EntryT& entry = items[index];

                    if (grid)
                    {
                        Texture icon = ItemData::get(entry.itemid).get_icon(false);
                        icon.shift(Point<int16_t>(0, 32));
                        icon.draw(iconpos);
                    }
                    else
                    {
                        const Texture& icon = ItemData::get(entry.itemid).get_icon(false);
                        icon.draw(iconpos);
                    }

                    if (tab != InventoryType::EQUIP && entry.count > 1)
                    {
                        countset.draw(std::to_string(entry.count), iconpos + Point<int16_t>(0, 20));
                    }
                }
                else
                {
                    disabled_slot.draw(cellpos + storage_nodes::empty_slot_shift());
                }

                if (grid && index == selection)
                {
                    cell_highlight().draw(cellpos);
                }
            }
        }
    }

    UIStorage::UIStorage(const Inventory& in_inventory)
        : inventory(in_inventory),
          tab(InventoryType::EQUIP),
          storage_offset(0),
          inventory_offset(0),
          storage_selection(-1),
          inventory_selection(-1),
          last_cursor_pos(),
          storage_meso(0),
          slots(0),
          npcid(0)
    {
        nl::node src = storage_nodes::src();

        sprites.emplace_back(src["backgrnd"]);
        // v83 ships a single merged background layer.
        if (nl::node backgrnd2 = src["backgrnd2"])
        {
            sprites.emplace_back(backgrnd2);
            sprites.emplace_back(src["backgrnd3"]);
        }

        buttons[GET] = std::make_unique<MapleButton>(src["BtGet"], storage_nodes::get_pos());
        buttons[PUT] = std::make_unique<MapleButton>(src["BtPut"], storage_nodes::put_pos());
        buttons[SORT] = std::make_unique<MapleButton>(src["BtSort"], storage_nodes::sort_pos());
        buttons[IN_COIN] = std::make_unique<MapleButton>(src["BtInCoin"], storage_nodes::in_coin_pos());
        buttons[OUT_COIN] = std::make_unique<MapleButton>(src["BtOutCoin"], storage_nodes::out_coin_pos());
        buttons[EXIT] = std::make_unique<MapleButton>(src["BtExit"], storage_nodes::exit_pos());

        // Full/Small mode and slot expansion only exist post-Chaos and their
        // UI flows are not implemented; keep the controls hidden.
        if (storage_nodes::has_mode_buttons())
        {
            buttons[FULL] = std::make_unique<MapleButton>(src["BtFull"]);
            buttons[SMALL] = std::make_unique<MapleButton>(src["BtSmall"]);
            buttons[SLOT_EX] = std::make_unique<MapleButton>(src["BtSlotEx"]);
            buttons[FULL]->set_active(false);
            buttons[SMALL]->set_active(false);
            buttons[SLOT_EX]->set_active(false);
        }

        nl::node tab_enabled = src["Tab"]["enabled"];
        nl::node tab_disabled = src["Tab"]["disabled"];

        buttons[TAB_EQUIP] = std::make_unique<TwoSpriteButton>(tab_disabled["0"], tab_enabled["0"], storage_nodes::tab_pos(false, 0));
        buttons[TAB_USE] = std::make_unique<TwoSpriteButton>(tab_disabled["1"], tab_enabled["1"], storage_nodes::tab_pos(false, 1));
        buttons[TAB_ETC] = std::make_unique<TwoSpriteButton>(tab_disabled["2"], tab_enabled["2"], storage_nodes::tab_pos(false, 2));
        buttons[TAB_SETUP] = std::make_unique<TwoSpriteButton>(tab_disabled["3"], tab_enabled["3"], storage_nodes::tab_pos(false, 3));
        buttons[TAB_CASH] = std::make_unique<TwoSpriteButton>(tab_disabled["4"], tab_enabled["4"], storage_nodes::tab_pos(false, 4));

        // The v83 header carries a second tab strip over the inventory
        // column; both strips drive the same tab selection.
        if (!storage_nodes::post_chaos())
        {
            buttons[TAB2_EQUIP] = std::make_unique<TwoSpriteButton>(tab_disabled["0"], tab_enabled["0"], storage_nodes::tab_pos(true, 0));
            buttons[TAB2_USE] = std::make_unique<TwoSpriteButton>(tab_disabled["1"], tab_enabled["1"], storage_nodes::tab_pos(true, 1));
            buttons[TAB2_ETC] = std::make_unique<TwoSpriteButton>(tab_disabled["2"], tab_enabled["2"], storage_nodes::tab_pos(true, 2));
            buttons[TAB2_SETUP] = std::make_unique<TwoSpriteButton>(tab_disabled["3"], tab_enabled["3"], storage_nodes::tab_pos(true, 3));
            buttons[TAB2_CASH] = std::make_unique<TwoSpriteButton>(tab_disabled["4"], tab_enabled["4"], storage_nodes::tab_pos(true, 4));
        }

        // Slot hitboxes: one AreaButton per visible cell, laid out by the
        // generation (grid cells vs legacy list rows).
        const int16_t storage_base = storage_nodes::post_chaos() ? STORAGE_CELL0 : STORAGE_ROW0;
        const int16_t inventory_base = storage_nodes::post_chaos() ? INVENTORY_CELL0 : INVENTORY_ROW0;
        const Point<int16_t> cell_dim = storage_nodes::post_chaos()
            ? Point<int16_t>(storage_nodes::GRID_CELL_SIZE, storage_nodes::GRID_CELL_SIZE)
            : Point<int16_t>(storage_nodes::row_width(), storage_nodes::row_height());

        for (int16_t i = 0; i < storage_nodes::cells_visible(); ++i)
        {
            buttons[storage_base + i] = std::make_unique<AreaButton>(
                storage_nodes::cell_pos(false, i),
                cell_dim
            );

            buttons[inventory_base + i] = std::make_unique<AreaButton>(
                storage_nodes::cell_pos(true, i),
                cell_dim
            );
        }

        buttons[button_by_tab(tab)]->set_state(Button::PRESSED);

        selection = storage_nodes::selection();
        disabled_slot = storage_nodes::empty_slot();
        meso_icon = storage_nodes::meso_icon();

        storage_mesolabel = { Text::A11M, Text::RIGHT, Text::LIGHTGREY };
        player_mesolabel = { Text::A11M, Text::RIGHT, Text::LIGHTGREY };

        for (auto type : INVENTORY_TABS)
        {
            storage_items[type].clear();
        }

        refresh_inventory_tab();

        dimension = Texture(src["backgrnd"]).get_dimensions();
        update_screen(Constants::viewwidth(), Constants::viewheight());
        active = false;
    }

    void UIStorage::update_screen(int16_t, int16_t)
    {
        position = ui_layout::origin() + Point<int16_t>(
            400 - dimension.x() / 2,
            240 - dimension.y() / 2
        );
    }

    void UIStorage::draw(float alpha) const
    {
        UIElement::draw(alpha);

        static const Charset countset = { nl::nx::ui["Basic.img"]["ItemNo"], Charset::LEFT };

        draw_panel(
            false,
            position,
            storage_items[tab],
            storage_offset,
            storage_selection,
            tab,
            selection,
            disabled_slot,
            countset
        );

        draw_panel(
            true,
            position,
            inventory_items,
            inventory_offset,
            inventory_selection,
            tab,
            selection,
            disabled_slot,
            countset
        );

        meso_icon.draw(position + storage_nodes::storage_meso_icon_pos());
        meso_icon.draw(position + storage_nodes::player_meso_icon_pos());
        storage_mesolabel.draw(position + storage_nodes::storage_meso_label_pos());
        player_mesolabel.draw(position + storage_nodes::player_meso_label_pos());
    }

    void UIStorage::update()
    {
        int64_t player_meso = inventory.get_meso();

        std::string storage_mesostr = std::to_string(storage_meso);
        std::string player_mesostr = std::to_string(player_meso);
        string_format::split_number(storage_mesostr);
        string_format::split_number(player_mesostr);

        storage_mesolabel.change_text(storage_mesostr);
        player_mesolabel.change_text(player_mesostr);
    }

    bool UIStorage::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        return UIElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIStorage::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        Point<int16_t> cursoroffset = cursorpos - position;
        last_cursor_pos = cursoroffset;

        int16_t cell = storage_nodes::cell_at(false, cursoroffset);
        if (cell >= 0)
        {
            show_item(cell, true);
        }
        else
        {
            cell = storage_nodes::cell_at(true, cursoroffset);
            if (cell >= 0)
            {
                show_item(cell, false);
            }
            else
            {
                clear_tooltip();
            }
        }

        return UIElement::send_cursor(clicked, cursorpos);
    }

    void UIStorage::send_scroll(double yoffset)
    {
        int16_t step = static_cast<int16_t>(storage_nodes::grid_scroll_step() * (yoffset > 0 ? -1 : 1));

        if (storage_nodes::cell_at(false, last_cursor_pos) >= 0)
        {
            storage_offset = static_cast<int16_t>(storage_offset + step);
        }
        else if (storage_nodes::cell_at(true, last_cursor_pos) >= 0)
        {
            inventory_offset = static_cast<int16_t>(inventory_offset + step);
        }

        clamp_offsets();
    }

    void UIStorage::rightclick(Point<int16_t> cursorpos)
    {
        Point<int16_t> cursoroffset = cursorpos - position;

        int16_t cell = storage_nodes::cell_at(false, cursoroffset);
        if (cell >= 0)
        {
            int16_t index = static_cast<int16_t>(cell + storage_offset);
            if (index >= 0 && index < static_cast<int16_t>(storage_items[tab].size()))
            {
                storage_selection = index;
                inventory_selection = -1;
                request_take_selected();
            }
            return;
        }

        cell = storage_nodes::cell_at(true, cursoroffset);
        if (cell >= 0)
        {
            int16_t index = static_cast<int16_t>(cell + inventory_offset);
            if (index >= 0 && index < static_cast<int16_t>(inventory_items.size()))
            {
                inventory_selection = index;
                storage_selection = -1;
                request_store_selected();
            }
        }
    }

    void UIStorage::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            close_with_packet();
        }
    }

    void UIStorage::open(int32_t new_npcid, uint8_t new_slots, int32_t meso)
    {
        npcid = new_npcid;
        slots = new_slots;
        storage_meso = meso;

        storage_offset = 0;
        inventory_offset = 0;
        storage_selection = -1;
        inventory_selection = -1;

        for (auto type : INVENTORY_TABS)
        {
            storage_items[type].clear();
        }

        refresh_inventory_tab();
        active = true;
    }

    void UIStorage::close()
    {
        clear_tooltip();
        active = false;
        storage_selection = -1;
        inventory_selection = -1;
    }

    void UIStorage::set_items_for_all(const EnumMap<InventoryType::Id, std::vector<ItemEntry>>& items)
    {
        for (auto type : INVENTORY_TABS)
        {
            storage_items[type] = items[type];
        }

        clamp_offsets();
    }

    void UIStorage::set_items_for_tab(InventoryType::Id type, const std::vector<ItemEntry>& items)
    {
        storage_items[type] = items;
        clamp_offsets();
    }

    void UIStorage::set_slots(uint8_t value)
    {
        slots = value;
    }

    void UIStorage::set_meso(int32_t meso)
    {
        storage_meso = meso;
    }

    void UIStorage::modify(InventoryType::Id type)
    {
        if (type == tab)
        {
            refresh_inventory_tab();
        }
    }

    Button::State UIStorage::button_pressed(uint16_t buttonid)
    {
        clear_tooltip();

        const int16_t storage_base = storage_nodes::post_chaos() ? STORAGE_CELL0 : STORAGE_ROW0;
        const int16_t inventory_base = storage_nodes::post_chaos() ? INVENTORY_CELL0 : INVENTORY_ROW0;

        if (buttonid >= storage_base && buttonid < storage_base + storage_nodes::cells_visible())
        {
            int16_t visible_slot = static_cast<int16_t>(buttonid - storage_base);
            int16_t slot = static_cast<int16_t>(visible_slot + storage_offset);
            if (slot >= 0 && slot < static_cast<int16_t>(storage_items[tab].size()))
            {
                if (slot == storage_selection)
                {
                    request_take_selected();
                }
                else
                {
                    storage_selection = slot;
                    inventory_selection = -1;
                }
            }
            return Button::NORMAL;
        }

        if (buttonid >= inventory_base && buttonid < inventory_base + storage_nodes::cells_visible())
        {
            int16_t visible_slot = static_cast<int16_t>(buttonid - inventory_base);
            int16_t slot = static_cast<int16_t>(visible_slot + inventory_offset);
            if (slot >= 0 && slot < static_cast<int16_t>(inventory_items.size()))
            {
                if (slot == inventory_selection)
                {
                    request_store_selected();
                }
                else
                {
                    inventory_selection = slot;
                    storage_selection = -1;
                }
            }
            return Button::NORMAL;
        }

        switch (buttonid)
        {
        case GET:
            request_take_selected();
            return Button::NORMAL;
        case PUT:
            request_store_selected();
            return Button::NORMAL;
        case SORT:
            request_sort();
            return Button::NORMAL;
        case IN_COIN:
            request_meso_change(true);
            return Button::NORMAL;
        case OUT_COIN:
            request_meso_change(false);
            return Button::NORMAL;
        case EXIT:
            close_with_packet();
            return Button::PRESSED;
        case TAB_EQUIP:
        case TAB2_EQUIP:
            change_tab(InventoryType::EQUIP);
            return Button::IDENTITY;
        case TAB_USE:
        case TAB2_USE:
            change_tab(InventoryType::USE);
            return Button::IDENTITY;
        case TAB_ETC:
        case TAB2_ETC:
            change_tab(InventoryType::ETC);
            return Button::IDENTITY;
        case TAB_SETUP:
        case TAB2_SETUP:
            change_tab(InventoryType::SETUP);
            return Button::IDENTITY;
        case TAB_CASH:
        case TAB2_CASH:
            change_tab(InventoryType::CASH);
            return Button::IDENTITY;
        default:
            return Button::PRESSED;
        }
    }

    void UIStorage::clear_tooltip()
    {
        UI::get().clear_tooltip(Tooltip::SHOP);
    }

    void UIStorage::show_item(int16_t visible_slot, bool storage_side)
    {
        int16_t index = static_cast<int16_t>(visible_slot + (storage_side ? storage_offset : inventory_offset));

        if (storage_side)
        {
            const auto& items = storage_items[tab];
            if (index < 0 || index >= static_cast<int16_t>(items.size()))
            {
                clear_tooltip();
                return;
            }

            UI::get().show_item(Tooltip::SHOP, items[index].itemid);
            return;
        }

        if (index < 0 || index >= static_cast<int16_t>(inventory_items.size()))
        {
            clear_tooltip();
            return;
        }

        UI::get().show_item(Tooltip::SHOP, inventory_items[index].itemid);
    }

    void UIStorage::refresh_inventory_tab()
    {
        inventory_items.clear();

        int16_t slotmax = inventory.get_slotmax(tab);
        for (int16_t slot = 1; slot <= slotmax; ++slot)
        {
            int32_t itemid = inventory.get_item_id(tab, slot);
            if (itemid == 0)
            {
                continue;
            }

            int16_t count = tab == InventoryType::EQUIP ?
                1 :
                inventory.get_item_count(tab, slot);

            inventory_items.push_back({ itemid, count, slot });
        }

        clamp_offsets();
    }

    void UIStorage::clamp_offsets()
    {
        const auto& storage_tab_items = storage_items[tab];

        storage_offset = std::clamp<int16_t>(storage_offset, 0, max_offset(storage_tab_items.size()));
        inventory_offset = std::clamp<int16_t>(inventory_offset, 0, max_offset(inventory_items.size()));

        if (storage_selection >= static_cast<int16_t>(storage_tab_items.size()))
        {
            storage_selection = -1;
        }

        if (inventory_selection >= static_cast<int16_t>(inventory_items.size()))
        {
            inventory_selection = -1;
        }
    }

    void UIStorage::change_tab(InventoryType::Id type)
    {
        uint16_t oldtab = button_by_tab(tab);
        uint16_t newtab = button_by_tab(type);

        buttons[oldtab]->set_state(Button::NORMAL);
        buttons[newtab]->set_state(Button::PRESSED);

        uint16_t oldmirror = static_cast<uint16_t>(oldtab + TAB2_EQUIP - TAB_EQUIP);
        uint16_t newmirror = static_cast<uint16_t>(newtab + TAB2_EQUIP - TAB_EQUIP);
        if (buttons.find(oldmirror) != buttons.end())
        {
            buttons[oldmirror]->set_state(Button::NORMAL);
            buttons[newmirror]->set_state(Button::PRESSED);
        }

        tab = type;
        storage_offset = 0;
        inventory_offset = 0;
        storage_selection = -1;
        inventory_selection = -1;

        refresh_inventory_tab();
    }

    void UIStorage::request_take_selected()
    {
        const auto& items = storage_items[tab];
        if (storage_selection < 0 || storage_selection >= static_cast<int16_t>(items.size()))
        {
            return;
        }

        StorageActionPacket(tab, static_cast<uint8_t>(storage_selection)).dispatch();
    }

    void UIStorage::request_store_selected()
    {
        if (inventory_selection < 0 || inventory_selection >= static_cast<int16_t>(inventory_items.size()))
        {
            return;
        }

        const InventoryEntry entry = inventory_items[inventory_selection];
        int16_t max_quantity = std::max<int16_t>(1, entry.count);

        if (tab != InventoryType::EQUIP && max_quantity > 1)
        {
            auto on_enter = [entry](int32_t quantity) {
                StorageActionPacket(
                    entry.slot,
                    entry.itemid,
                    static_cast<int16_t>(quantity)
                ).dispatch();
            };
            UI::get().emplace<UIEnterNumber>(
                "How many would you like to store?",
                on_enter,
                1,
                max_quantity,
                1
            );
            return;
        }

        StorageActionPacket(entry.slot, entry.itemid, 1).dispatch();
    }

    void UIStorage::request_sort()
    {
        StorageActionPacket::sort().dispatch();
    }

    void UIStorage::request_meso_change(bool store_to_storage)
    {
        int32_t max_value = 0;
        if (store_to_storage)
        {
            int64_t player_meso = inventory.get_meso();
            int64_t capped = std::min<int64_t>(player_meso, std::numeric_limits<int32_t>::max());
            max_value = static_cast<int32_t>(std::max<int64_t>(0, capped));
        }
        else
        {
            max_value = std::max<int32_t>(0, storage_meso);
        }

        if (max_value <= 0)
        {
            return;
        }

        auto on_enter = [store_to_storage](int32_t amount) {
            int32_t signed_amount = store_to_storage ? -amount : amount;
            StorageActionPacket(signed_amount).dispatch();
        };

        UI::get().emplace<UIEnterNumber>(
            store_to_storage ? "How many mesos would you like to store?" : "How many mesos would you like to take out?",
            on_enter,
            1,
            max_value,
            1
        );
    }

    void UIStorage::close_with_packet()
    {
        close();
        StorageActionPacket::leave().dispatch();
    }

    uint16_t UIStorage::button_by_tab(InventoryType::Id type) const
    {
        switch (type)
        {
        case InventoryType::EQUIP:
            return TAB_EQUIP;
        case InventoryType::USE:
            return TAB_USE;
        case InventoryType::ETC:
            return TAB_ETC;
        case InventoryType::SETUP:
            return TAB_SETUP;
        case InventoryType::CASH:
            return TAB_CASH;
        default:
            return TAB_EQUIP;
        }
    }
}
