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
#include "UIEquipInventory.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Pet/PetAutomation.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/InventoryPackets.h"

#include "UIEquipInventoryNodes.h"
#include "nlnx/nx.hpp"

#include <array>

namespace jrc
{
    UIEquipInventory::UIEquipInventory(const Inventory& invent) :
        UIDragElement<PosEQINV>(equipinv_nodes::window_position()), inventory(invent) {

        iconpositions = equipinv_nodes::icon_positions();

        nl::node source;
        nl::node petsource;
        if (equipinv_nodes::post_chaos())
        {
            // post-Chaos splits the window into the layered character
            // panel and separate pet art under UIWindow2.img.
            source    = nl::nx::ui["UIWindow2.img"]["Equip"]["character"];
            petsource = nl::nx::ui["UIWindow2.img"]["Equip"]["pet"];

            sprites.emplace_back(source["backgrnd"]);
            sprites.emplace_back(source["backgrnd2"]);
            sprites.emplace_back(source["backgrnd3_Kanna"]);
            sprites.emplace_back(source["cashPendant"]);
            sprites.emplace_back(source["charmPocket"]);
            sprites.emplace_back(source["emblem"]);

            buttons[BT_TOGGLEPETS] = std::make_unique<MapleButton>(source["BtPet"]);

            textures_pet.emplace_back(petsource["backgrnd"]);
            textures_pet.emplace_back(petsource["backgrnd2"]);
            textures_pet.emplace_back(petsource["backgrnd3"]);
        }
        else
        {
            // v83 keeps the whole character panel in one bitmap
            // (UIWindow.img/Equip/backgrnd); the pet panel is the
            // separate "pet" sub node attached to the right edge.
            source    = nl::nx::ui["UIWindow.img"]["Equip"];
            petsource = source["pet"];

            sprites.emplace_back(source["backgrnd"]);

            buttons[BT_TOGGLEPETS] = std::make_unique<MapleButton>(
                source["BtPetEquipShow"], Point<int16_t>(87, 287));

            textures_pet.emplace_back(petsource);
        }

        load_icons();

        dimension = equipinv_nodes::dimension();
        active = true;
        showpetequips = false;
    }

    void UIEquipInventory::draw(float alpha) const
    {
        UIElement::draw(alpha);

        for (auto iter : icons)
        {
            if (iter.second)
                iter.second->draw(position + iconpositions[iter.first]);
        }

        if (showpetequips)
        {
            Point<int16_t> position_pet(position + equipinv_nodes::pet_panel_origin());
            for (auto& texture : textures_pet)
            {
                texture.draw(position_pet);
            }

            const auto& automation = Stage::get().get_pet_automation();
            draw_pet_item(automation.get_hp_item(), equipinv_nodes::pet_hp_pos());
            draw_pet_item(automation.get_mp_item(), equipinv_nodes::pet_mp_pos());

            constexpr std::array<int32_t, 5> ABILITIES = {
                pet_automation::MESO_MAGNET,
                pet_automation::ITEM_POUCH,
                pet_automation::AUTO_HP_POUCH,
                pet_automation::AUTO_MP_POUCH,
                pet_automation::WING_BOOTS
            };
            const std::array<Point<int16_t>, 5> POSITIONS =
                equipinv_nodes::pet_ability_positions();
            const pet_capability::Capabilities capabilities =
                Stage::get().get_pet_capabilities();
            const std::array<bool, 5> ENABLED = {
                capabilities.meso_magnet, capabilities.item_pouch,
                capabilities.auto_hp_pouch, capabilities.auto_mp_pouch,
                capabilities.wing_boots
            };
            for (size_t index = 0; index < ABILITIES.size(); ++index)
                if (ENABLED[index])
                    draw_pet_item(ABILITIES[index], POSITIONS[index]);
        }
    }

    Button::State UIEquipInventory::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_TOGGLEPETS:
            showpetequips = !showpetequips;
            return Button::NORMAL;
        default:
            return Button::PRESSED;
        }
    }

    bool UIEquipInventory::is_in_range(Point<int16_t> cursorpos) const
    {
        if (UIElement::is_in_range(cursorpos))
            return true;
        return showpetequips
            && equipinv_nodes::pet_panel_bounds(position).contains(cursorpos);
    }

    void UIEquipInventory::update_slot(Equipslot::Id slot)
    {
        if (int32_t item_id = inventory.get_item_id(InventoryType::EQUIPPED, slot))
        {
            const Texture& texture = ItemData::get(item_id).get_icon(false);
            icons[slot] = std::make_unique<Icon>(
                std::make_unique<EquipIcon>(slot),
                texture,
                -1
                );
        }
        else if (icons[slot])
        {
            icons[slot].release();
        }

        clear_tooltip();
    }

    void UIEquipInventory::load_icons()
    {
        icons.clear();

        for (auto iter : Equipslot::values)
        {
            update_slot(iter);
        }
    }

    UIElement::CursorResult UIEquipInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            clear_tooltip();
            return UIDragElement::send_cursor(pressed, cursorpos);
        }

        Equipslot::Id slot = slot_by_position(cursorpos);
        if (auto icon = icons[slot].get())
        {
            if (pressed)
            {
                icon->start_drag(cursorpos - position - iconpositions[slot]);
                UI::get().drag_icon(icon);

                clear_tooltip();
                return { Cursor::GRABBING, true };
            }
            else
            {
                show_equip(slot);
                return { Cursor::CANGRAB, true };
            }
        }

        clear_tooltip();
        return UIDragElement::send_cursor(pressed, cursorpos);
    }

    void UIEquipInventory::doubleclick(Point<int16_t> cursorpos)
    {
        Equipslot::Id slot = slot_by_position(cursorpos);
        if (icons[slot])
        {
            if (int16_t freeslot = inventory.find_free_slot(InventoryType::EQUIP))
            {
                UnequipItemPacket(slot, freeslot).dispatch();
            }
        }
    }

    void UIEquipInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
    {
        if (int8_t potion = pet_potion_by_position(cursorpos))
        {
            icon.drop_on_pet_potion(potion == 1);
            return;
        }
        if (Equipslot::Id slot = slot_by_position(cursorpos))
        {
            icon.drop_on_equips(slot);
        }
    }

    int8_t UIEquipInventory::pet_potion_by_position(Point<int16_t> cursorpos) const
    {
        if (!showpetequips)
            return 0;
        return equipinv_nodes::pet_hp_bounds(position).contains(cursorpos) ? 1
             : equipinv_nodes::pet_mp_bounds(position).contains(cursorpos) ? 2
             : 0;
    }

    void UIEquipInventory::draw_pet_item(int32_t item_id, Point<int16_t> relative) const
    {
        if (item_id <= 0)
            return;
        const ItemData& item = ItemData::get(item_id);
        if (item)
            item.get_icon(false).draw(position + relative + Point<int16_t>(0, 32));
    }

    void UIEquipInventory::toggle_active()
    {
        clear_tooltip();
        UIElement::toggle_active();
    }

    void UIEquipInventory::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            toggle_active();
        }
    }

    void UIEquipInventory::modify(int16_t pos, int8_t mode, int16_t arg)
    {
        Equipslot::Id eqpos = Equipslot::by_id(pos);
        Equipslot::Id eqarg = Equipslot::by_id(arg);
        switch (mode)
        {
        case 0:
        case 3:
            update_slot(eqpos);
            break;
        case 2:
            update_slot(eqpos);
            update_slot(eqarg);
            break;
        }
    }

    void UIEquipInventory::show_equip(Equipslot::Id slot)
    {
        UI::get().show_equip(Tooltip::EQUIPINVENTORY, slot);
    }

    void UIEquipInventory::clear_tooltip()
    {
        UI::get().clear_tooltip(Tooltip::EQUIPINVENTORY);
    }

    Equipslot::Id UIEquipInventory::slot_by_position(Point<int16_t> cursorpos) const
    {
        for (auto iter : iconpositions)
        {
            Rectangle<int16_t> iconrect = Rectangle<int16_t>(
                position + iter.second,
                position + iter.second + Point<int16_t>(32, 32)
                );
            if (iconrect.contains(cursorpos))
            {
                return iter.first;
            }
        }
        return Equipslot::NONE;
    }


    UIEquipInventory::EquipIcon::EquipIcon(int16_t s)
    {
        source = s;
    }

    void UIEquipInventory::EquipIcon::drop_on_stage() const
    {
        UnequipItemPacket(source, 0).dispatch();
    }

    void UIEquipInventory::EquipIcon::drop_on_items(InventoryType::Id tab, Equipslot::Id eqslot, int16_t slot, bool equip) const
    {
        if (tab != InventoryType::EQUIP)
            return;

        if (equip)
        {
            if (eqslot == source)
            {
                EquipItemPacket(slot, eqslot)
                    .dispatch();
            }
        }
        else
        {
            UnequipItemPacket(source, slot)
                .dispatch();
        }
    }
}
