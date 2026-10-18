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

#include "nlnx/nx.hpp"

#include <array>

namespace jrc
{
    UIEquipInventory::UIEquipInventory(const Inventory& invent) :
        UIDragElement<PosEQINV>(Point<int16_t>(175, 20)), inventory(invent) {

        // v83 keeps the whole character panel in one bitmap
        // (UIWindow.img/Equip/backgrnd).  The labelled sockets form the
        // same five-column grid the later files use (32px cells, 33px
        // pitch) shifted by (-5, +10): columns x = 5, 38, 71, 104, 137
        // and rows y = 35, 68, 101, 134, 167, 200, 233.  Only cells that
        // actually carry a socket in the bitmap are mapped; the four
        // same-labelled sockets at (137,200) and y = 233 (x = 71, 104,
        // 137) have no counterpart slot in this client and stay unused.
        iconpositions[Equipslot::CAP]      = Point<int16_t>( 38,  35);
        iconpositions[Equipslot::MEDAL]    = Point<int16_t>(  5,  68);
        iconpositions[Equipslot::EYEACC]   = Point<int16_t>( 38,  68);
        iconpositions[Equipslot::RING3]    = Point<int16_t>(104,  68);
        iconpositions[Equipslot::RING4]    = Point<int16_t>(137,  68);
        iconpositions[Equipslot::FACEACC]  = Point<int16_t>( 71, 101);
        iconpositions[Equipslot::EARRINGS] = Point<int16_t>(104, 101);
        iconpositions[Equipslot::CAPE]     = Point<int16_t>(  5, 134);
        iconpositions[Equipslot::TOP]      = Point<int16_t>( 38, 134);
        iconpositions[Equipslot::PENDANT]  = Point<int16_t>( 71, 134);
        iconpositions[Equipslot::WEAPON]   = Point<int16_t>(104, 134);
        iconpositions[Equipslot::SHIELD]   = Point<int16_t>(137, 134);
        iconpositions[Equipslot::GLOVES]   = Point<int16_t>(  5, 167);
        iconpositions[Equipslot::PANTS]    = Point<int16_t>( 38, 167);
        iconpositions[Equipslot::BELT]     = Point<int16_t>( 71, 167);
        iconpositions[Equipslot::RING]     = Point<int16_t>(104, 167);
        iconpositions[Equipslot::RING2]    = Point<int16_t>(137, 167);
        iconpositions[Equipslot::SHOES]    = Point<int16_t>( 71, 200);
        iconpositions[Equipslot::TAMEDMOB] = Point<int16_t>(  5, 233);
        iconpositions[Equipslot::SADDLE]   = Point<int16_t>( 38, 233);

        nl::node source = nl::nx::ui["UIWindow.img"]["Equip"];
        nl::node petsource = source["pet"];

        // The character/pet sub-windows and the backgrnd2/backgrnd3
        // layers of the post-Chaos file do not exist here; the pet
        // panel is the separate 177x181 "pet" bitmap attached to the
        // right edge of the 175px-wide window.
        sprites.emplace_back(source["backgrnd"]);

        buttons[BT_TOGGLEPETS] = std::make_unique<MapleButton>(
            source["BtPetEquipShow"], Point<int16_t>(87, 287));

        textures_pet.emplace_back(petsource);

        load_icons();

        dimension = { 175, 304 };
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
            // The pet panel attaches flush to the right edge of the
            // 175px-wide v83 window; its sockets sit on a 4-column grid
            // (x = 13, 46, 79, 112) with rows at y = 44, 77 and 110.
            Point<int16_t> position_pet(position + Point<int16_t>(175, 0));
            for (auto& texture : textures_pet)
            {
                texture.draw(position_pet);
            }

            const auto& automation = Stage::get().get_pet_automation();
            draw_pet_item(automation.get_hp_item(), { 188, 44 });
            draw_pet_item(automation.get_mp_item(), { 221, 44 });

            constexpr std::array<int32_t, 5> ABILITIES = {
                pet_automation::MESO_MAGNET,
                pet_automation::ITEM_POUCH,
                pet_automation::AUTO_HP_POUCH,
                pet_automation::AUTO_MP_POUCH,
                pet_automation::WING_BOOTS
            };
            constexpr std::array<Point<int16_t>, 5> POSITIONS = {
                Point<int16_t>(188, 77), Point<int16_t>(221, 77),
                Point<int16_t>(254, 77), Point<int16_t>(204, 110),
                Point<int16_t>(237, 110)
            };
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
        return showpetequips && Rectangle<int16_t>(
            position + Point<int16_t>(175, 0),
            position + Point<int16_t>(352, 181)
        ).contains(cursorpos);
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
        Rectangle<int16_t> hp(position + Point<int16_t>(188, 44),
            position + Point<int16_t>(220, 76));
        Rectangle<int16_t> mp(position + Point<int16_t>(221, 44),
            position + Point<int16_t>(253, 76));
        return hp.contains(cursorpos) ? 1 : mp.contains(cursorpos) ? 2 : 0;
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
