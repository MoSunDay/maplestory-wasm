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
#include "UICharCreation.h"

#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../UITypes/UILoginNotice.h"
#include "../UITypes/UICharSelect.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Net/Packets/CharCreationPackets.h"
#include "CharacterCreation/NamePolicy.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace
    {
        constexpr uint32_t REQUEST_TIMEOUT = 8000;
    }

    UICharcreation::UICharcreation()
    {
        nl::node src = nl::nx::ui["Login.img"];
        nl::node bgsrc = nl::nx::map["Back"]["login.img"]["back"];
        nl::node crsrc = src["NewChar"];

        sky = bgsrc["2"];
        cloud = bgsrc["27"];

        sprites.emplace_back(bgsrc["15"], Point<int16_t>(153, 685));
        sprites.emplace_back(bgsrc["16"], Point<int16_t>(200, 400));
        sprites.emplace_back(bgsrc["17"], Point<int16_t>(160, 263));
        sprites.emplace_back(bgsrc["18"], Point<int16_t>(349, 1220));
        sprites.emplace_back(src["Common"]["frame"], Point<int16_t>(400, 300));

        nameboard = crsrc["charName"];

        sprites_lookboard.emplace_back(crsrc["charSet"], Point<int16_t>(450, 115));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["0"]["normal"], Point<int16_t>(461, 217));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["1"]["normal"], Point<int16_t>(461, 236));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["2"]["normal"], Point<int16_t>(461, 255));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["3"]["normal"], Point<int16_t>(461, 274));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["4"]["normal"], Point<int16_t>(461, 293));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["5"]["normal"], Point<int16_t>(461, 312));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["6"]["normal"], Point<int16_t>(461, 331));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["7"]["normal"], Point<int16_t>(461, 350));
        sprites_lookboard.emplace_back(crsrc["avatarSel"]["8"]["normal"], Point<int16_t>(461, 369));

        buttons[BT_CHARC_OK] = std::make_unique<MapleButton>(crsrc["BtYes"], Point<int16_t>(482, 292));
        buttons[BT_CHARC_CANCEL] = std::make_unique<MapleButton>(crsrc["BtNo"], Point<int16_t>(555, 292));
        buttons[BT_CHARC_FACEL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 216));
        buttons[BT_CHARC_FACER] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 216));
        buttons[BT_CHARC_HAIRL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 235));
        buttons[BT_CHARC_HAIRR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 235));
        buttons[BT_CHARC_HAIRCL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 254));
        buttons[BT_CHARC_HAIRCR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 254));
        buttons[BT_CHARC_SKINL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 273));
        buttons[BT_CHARC_SKINR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 273));
        buttons[BT_CHARC_TOPL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 292));
        buttons[BT_CHARC_TOPR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 292));
        buttons[BT_CHARC_BOTL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 311));
        buttons[BT_CHARC_BOTR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 311));
        buttons[BT_CHARC_SHOESL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 330));
        buttons[BT_CHARC_SHOESR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 330));
        buttons[BT_CHARC_WEPL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 349));
        buttons[BT_CHARC_WEPR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 348));
        buttons[BT_CHARC_GENDERL] = std::make_unique<MapleButton>(crsrc["BtLeft"], Point<int16_t>(521, 368));
        buttons[BT_CHARC_GEMDERR] = std::make_unique<MapleButton>(crsrc["BtRight"], Point<int16_t>(645, 368));

        buttons[BT_CHARC_FACEL]->set_active(false);
        buttons[BT_CHARC_FACER]->set_active(false);
        buttons[BT_CHARC_HAIRL]->set_active(false);
        buttons[BT_CHARC_HAIRR]->set_active(false);
        buttons[BT_CHARC_HAIRCL]->set_active(false);
        buttons[BT_CHARC_HAIRCR]->set_active(false);
        buttons[BT_CHARC_SKINL]->set_active(false);
        buttons[BT_CHARC_SKINR]->set_active(false);
        buttons[BT_CHARC_TOPL]->set_active(false);
        buttons[BT_CHARC_TOPR]->set_active(false);
        buttons[BT_CHARC_BOTL]->set_active(false);
        buttons[BT_CHARC_BOTR]->set_active(false);
        buttons[BT_CHARC_SHOESL]->set_active(false);
        buttons[BT_CHARC_SHOESR]->set_active(false);
        buttons[BT_CHARC_WEPL]->set_active(false);
        buttons[BT_CHARC_WEPR]->set_active(false);
        buttons[BT_CHARC_GENDERL]->set_active(false);
        buttons[BT_CHARC_GEMDERR]->set_active(false);

        namechar = { Text::A13M, Text::LEFT, Text::WHITE, { { 490, 219 }, { 630, 243 } }, 12 };
        namechar.set_state(Textfield::FOCUSED);

        facename = { Text::A11M, Text::CENTER, Text::BLACK };
        hairname = { Text::A11M, Text::CENTER, Text::BLACK };
        haircname = { Text::A11M, Text::CENTER, Text::BLACK };
        bodyname = { Text::A11M, Text::CENTER, Text::BLACK };
        topname = { Text::A11M, Text::CENTER, Text::BLACK };
        botname = { Text::A11M, Text::CENTER, Text::BLACK };
        shoename = { Text::A11M, Text::CENTER, Text::BLACK };
        wepname = { Text::A11M, Text::CENTER, Text::BLACK };
        gendername = { Text::A11M, Text::CENTER, Text::BLACK };

        nl::node mkinfo = nl::nx::etc["MakeCharInfo.img"]["Info"];
        for (int32_t i = 0; i < 2; i++)
        {
            bool f;
            nl::node mk_n;
            if (i == 0)
            {
                f = true;
                mk_n = mkinfo["CharFemale"];
            }
            else
            {
                f = false;
                mk_n = mkinfo["CharMale"];
            }

            for (auto subnode : mk_n)
            {
                int num = stoi(subnode.name());
                for (auto idnode : subnode)
                {
                    int value = idnode;
                    switch (num)
                    {
                    case 0:
                        faces[f].push_back(value);
                        break;
                    case 1:
                        hairs[f].push_back(value);
                        break;
                    case 2:
                        haircolors[f].push_back(static_cast<uint8_t>(value));
                        break;
                    case 3:
                        skins[f].push_back(static_cast<uint8_t>(value));
                        break;
                    case 4:
                        tops[f].push_back(value);
                        break;
                    case 5:
                        bots[f].push_back(value);
                        break;
                    case 6:
                        shoes[f].push_back(value);
                        break;
                    case 7:
                        weapons[f].push_back(value);
                        break;
                    }
                }
            }
        }

        female = false;
        randomize_look();

        newchar.set_direction(true);

        position = { 0, 0 };
        dimension = { 800, 600 };
        active = true;
        cloudfx = 200.0f;
    }

    void UICharcreation::randomize_look()
    {
        hair = randomizer.next_int(hairs[female].size());
        face = randomizer.next_int(faces[female].size());
        skin = randomizer.next_int(skins[female].size());
        haircolor = randomizer.next_int(haircolors[female].size());
        top = randomizer.next_int(tops[female].size());
        bot = randomizer.next_int(bots[female].size());
        shoe = randomizer.next_int(shoes[female].size());
        weapon = randomizer.next_int(weapons[female].size());

        newchar.set_body(skins[female][skin]);
        newchar.set_face(faces[female][face]);
        newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
        newchar.add_equip(tops[female][top]);
        newchar.add_equip(bots[female][bot]);
        newchar.add_equip(shoes[female][shoe]);
        newchar.add_equip(weapons[female][weapon]);

        bodyname.change_text(newchar.get_body()->get_name());
        facename.change_text(newchar.get_face()->get_name());
        hairname.change_text(newchar.get_hair()->get_name());
        haircname.change_text(newchar.get_hair()->getcolor());

        topname.change_text(get_equipname(Equipslot::TOP));
        botname.change_text(get_equipname(Equipslot::PANTS));
        shoename.change_text(get_equipname(Equipslot::SHOES));
        wepname.change_text(get_equipname(Equipslot::WEAPON));
        gendername.change_text(female ? "Female" : "Male");
    }

    Button::State UICharcreation::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_CHARC_OK:
            if (creation_flow.phase == CharacterCreation::Phase::CUSTOMIZING)
            {
                std::string cname = namechar.get_text();
                if (!CharacterCreation::is_locally_valid_name(cname))
                {
                    restore_name_entry();
                    UI::get().emplace<UILoginNotice>(UILoginNotice::ILLEGAL_NAME);
                    return Button::NORMAL;
                }

                // The initial availability result can become stale while the
                // player customizes the character. Recheck immediately before
                // CREATE_CHAR so the server can report an invalid/taken ID.
                creation_flow = CharacterCreation::checking_creation_name(cname);
                buttons[BT_CHARC_OK]->set_state(Button::DISABLED);
                NameCharPacket(cname).dispatch();
                return Button::DISABLED;
            }
            else if (creation_flow.phase != CharacterCreation::Phase::EDITING)
            {
                return Button::DISABLED;
            }
            else
            {
                std::string name = namechar.get_text();
                if (CharacterCreation::is_locally_valid_name(name))
                {
                    namechar.set_state(Textfield::NORMAL);
                    creation_flow = CharacterCreation::checking_name(name);
                    buttons[BT_CHARC_OK]->set_state(Button::DISABLED);
                    NameCharPacket(name).dispatch();
                    return Button::DISABLED;
                }
                else
                {
                    // Defer focus until after the current mouse press has
                    // finished dispatching; otherwise that same OK click is
                    // also routed outside the field and immediately blurs it.
                    namechar.set_state(Textfield::NORMAL);
                    focus_name_on_update = true;
                    UI::get().emplace<UILoginNotice>(UILoginNotice::ILLEGAL_NAME);
                    return Button::NORMAL;
                }
            }
        case BT_CHARC_CANCEL:
            if (CharacterCreation::shows_customization(creation_flow.phase))
            {
                restore_name_entry();
                return Button::NORMAL;
            }
            else
            {
                creation_flow = CharacterCreation::editing();
                focus_name_on_update = false;
                namechar.set_state(Textfield::NORMAL);
                active = false;
                if (auto charselect = UI::get().get_element<UICharSelect>())
                    charselect->makeactive();
                return Button::PRESSED;
            }
        }

        if (id >= BT_CHARC_FACEL && id <= BT_CHARC_GEMDERR)
        {
            switch (id)
            {
            case BT_CHARC_FACEL:
                face = (face > 0) ? face - 1 : faces[female].size() - 1;
                newchar.set_face(faces[female][face]);
                facename.change_text(newchar.get_face()->get_name());
                break;
            case BT_CHARC_FACER:
                face = (face < faces[female].size() - 1) ? face + 1 : 0;
                newchar.set_face(faces[female][face]);
                facename.change_text(newchar.get_face()->get_name());
                break;
            case BT_CHARC_HAIRL:
                hair = (hair > 0) ? hair - 1 : hairs[female].size() - 1;
                newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
                hairname.change_text(newchar.get_hair()->get_name());
                break;
            case BT_CHARC_HAIRR:
                hair = (hair < hairs[female].size() - 1) ? hair + 1 : 0;
                newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
                hairname.change_text(newchar.get_hair()->get_name());
                break;
            case BT_CHARC_HAIRCL:
                haircolor = (haircolor > 0) ? haircolor - 1 : haircolors[female].size() - 1;
                newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
                haircname.change_text(newchar.get_hair()->getcolor());
                break;
            case BT_CHARC_HAIRCR:
                haircolor = (haircolor < haircolors[female].size() - 1) ? haircolor + 1 : 0;
                newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
                haircname.change_text(newchar.get_hair()->getcolor());
                break;
            case BT_CHARC_SKINL:
                skin = (skin > 0) ? skin - 1 : skins[female].size() - 1;
                newchar.set_body(skins[female][skin]);
                bodyname.change_text(newchar.get_body()->get_name());
                break;
            case BT_CHARC_SKINR:
                skin = (skin < skins[female].size() - 1) ? skin + 1 : 0;
                newchar.set_body(skins[female][skin]);
                bodyname.change_text(newchar.get_body()->get_name());
                break;
            case BT_CHARC_TOPL:
                top = (top > 0) ? top - 1 : tops[female].size() - 1;
                newchar.add_equip(tops[female][top]);
                topname.change_text(get_equipname(Equipslot::TOP));
                break;
            case BT_CHARC_TOPR:
                top = (top < tops[female].size() - 1) ? top + 1 : 0;
                newchar.add_equip(tops[female][top]);
                topname.change_text(get_equipname(Equipslot::TOP));
                break;
            case BT_CHARC_BOTL:
                bot = (bot > 0) ? bot - 1 : bots[female].size() - 1;
                newchar.add_equip(bots[female][bot]);
                botname.change_text(get_equipname(Equipslot::PANTS));
                break;
            case BT_CHARC_BOTR:
                bot = (bot < bots[female].size() - 1) ? bot + 1 : 0;
                newchar.add_equip(bots[female][bot]);
                botname.change_text(get_equipname(Equipslot::PANTS));
                break;
            case BT_CHARC_SHOESL:
                shoe = (shoe > 0) ? shoe - 1 : shoes[female].size() - 1;
                newchar.add_equip(shoes[female][shoe]);
                shoename.change_text(get_equipname(Equipslot::SHOES));
                break;
            case BT_CHARC_SHOESR:
                shoe = (shoe < shoes[female].size() - 1) ? shoe + 1 : 0;
                newchar.add_equip(shoes[female][shoe]);
                shoename.change_text(get_equipname(Equipslot::SHOES));
                break;
            case BT_CHARC_WEPL:
                weapon = (weapon > 0) ? weapon - 1 : weapons[female].size() - 1;
                newchar.add_equip(weapons[female][weapon]);
                wepname.change_text(get_equipname(Equipslot::WEAPON));
                break;
            case BT_CHARC_WEPR:
                weapon = (weapon < weapons[female].size() - 1) ? weapon + 1 : 0;
                newchar.add_equip(weapons[female][weapon]);
                wepname.change_text(get_equipname(Equipslot::WEAPON));
                break;
            case BT_CHARC_GENDERL:
            case BT_CHARC_GEMDERR:
                female = !female;
                randomize_look();
                break;
            }
            return Button::MOUSEOVER;
        }
        return Button::PRESSED;
    }

    UIElement::CursorResult UICharcreation::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (Cursor::State new_state = namechar.send_cursor(cursorpos, clicked))
            return { new_state, true };

        return UIElement::send_cursor(clicked, cursorpos);
    }

    void UICharcreation::set_customization_controls(bool enabled)
    {
        buttons[BT_CHARC_OK]->set_position(enabled ? Point<int16_t>(486, 445) : Point<int16_t>(482, 292));
        buttons[BT_CHARC_CANCEL]->set_position(enabled ? Point<int16_t>(560, 445) : Point<int16_t>(555, 292));

        for (uint16_t id = BT_CHARC_FACEL; id <= BT_CHARC_GEMDERR; ++id)
        {
            buttons[id]->set_active(enabled);
        }
    }

    void UICharcreation::restore_name_entry()
    {
        creation_flow = CharacterCreation::editing();
        focus_name_on_update = false;
        set_customization_controls(false);
        buttons[BT_CHARC_OK]->set_state(Button::NORMAL);
        buttons[BT_CHARC_CANCEL]->set_state(Button::NORMAL);
        namechar.set_state(Textfield::FOCUSED);
    }

    void UICharcreation::restore_customization()
    {
        creation_flow = CharacterCreation::customizing();
        set_customization_controls(true);
        buttons[BT_CHARC_OK]->set_state(Button::NORMAL);
        buttons[BT_CHARC_CANCEL]->set_state(Button::NORMAL);
        namechar.set_state(Textfield::DISABLED);
    }

    void UICharcreation::dispatch_creation()
    {
        std::string cname = creation_flow.pending_name;
        creation_flow = CharacterCreation::creating(cname);
        buttons[BT_CHARC_OK]->set_state(Button::DISABLED);

        CreateCharPacket(
            cname,
            1,
            faces[female][face],
            hairs[female][hair],
            haircolors[female][haircolor],
            skins[female][skin],
            tops[female][top],
            bots[female][bot],
            shoes[female][shoe],
            weapons[female][weapon],
            female
        ).dispatch();
    }

    void UICharcreation::send_naming_result(const std::string& name, bool nameused)
    {
        CharacterCreation::NameResponseAction action =
            CharacterCreation::name_response_action(creation_flow, name, nameused);
        if (action == CharacterCreation::NameResponseAction::IGNORE)
        {
            return;
        }

        if (creation_flow.phase == CharacterCreation::Phase::CHECKING_NAME &&
            namechar.get_text() != creation_flow.pending_name)
        {
            restore_name_entry();
            return;
        }

        switch (action)
        {
        case CharacterCreation::NameResponseAction::REJECT_NAME:
        {
            restore_name_entry();
            UILoginNotice::Message message = CharacterCreation::is_locally_valid_name(name) ?
                UILoginNotice::NAME_IN_USE : UILoginNotice::ILLEGAL_NAME;
            UI::get().emplace<UILoginNotice>(message);
            break;
        }
        case CharacterCreation::NameResponseAction::ENTER_CUSTOMIZATION:
            restore_customization();
            break;
        case CharacterCreation::NameResponseAction::DISPATCH_CREATION:
            dispatch_creation();
            break;
        case CharacterCreation::NameResponseAction::RESTORE_CUSTOMIZATION:
            restore_customization();
            UI::get().emplace<UILoginNotice>(UILoginNotice::AN_ERROR_OCCURED);
            break;
        default:
            break;
        }
    }

    bool UICharcreation::handle_creation_failure()
    {
        if (creation_flow.phase != CharacterCreation::Phase::CREATING)
        {
            return false;
        }

        restore_customization();
        UI::get().emplace<UILoginNotice>(UILoginNotice::AN_ERROR_OCCURED);
        return true;
    }

    void UICharcreation::draw(float alpha) const
    {
        for (int16_t i = 0; i < 2; i++)
        {
            for (int16_t k = 0; k < 800; k += sky.width())
            {
                sky.draw(Point<int16_t>(k, (400 * i) - 100));
            }
        }

        int16_t cloudx = static_cast<int16_t>(cloudfx) % 800;
        cloud.draw(Point<int16_t>(cloudx - cloud.width(), 300));
        cloud.draw(Point<int16_t>(cloudx, 300));
        cloud.draw(Point<int16_t>(cloudx + cloud.width(), 300));

        if (!CharacterCreation::shows_customization(creation_flow.phase))
        {
            nameboard.draw(Point<int16_t>(455, 115 ));
            namechar.draw(position);
        }
        else
        {
            for (auto& sprite : sprites_lookboard)
            {
                sprite.draw(position, alpha);
            }
        }

        UIElement::draw(alpha);

        newchar.draw({ 360, 348 }, alpha);

        if (CharacterCreation::shows_customization(creation_flow.phase))
        {
            facename.draw(Point<int16_t>(591, 214));
            hairname.draw(Point<int16_t>(591, 233));
            haircname.draw(Point<int16_t>(591, 252));
            bodyname.draw(Point<int16_t>(591, 271));
            topname.draw(Point<int16_t>(591, 290));
            botname.draw(Point<int16_t>(591, 309));
            shoename.draw(Point<int16_t>(591, 328));
            wepname.draw(Point<int16_t>(591, 347));
            gendername.draw(Point<int16_t>(591, 366));
        }
    }

    void UICharcreation::update()
    {
        UIElement::update();

        if (CharacterCreation::shows_customization(creation_flow.phase))
        {
            for (auto& sprite : sprites_lookboard)
            {
                sprite.update();
            }
        }

        newchar.update(Constants::TIMESTEP);
        namechar.update(position);

        if (focus_name_on_update)
        {
            focus_name_on_update = false;
            namechar.set_state(Textfield::FOCUSED);
        }

        switch (creation_flow.phase)
        {
        case CharacterCreation::Phase::CHECKING_NAME:
        case CharacterCreation::Phase::CHECKING_CREATION_NAME:
        case CharacterCreation::Phase::CREATING:
        case CharacterCreation::Phase::RECOVERING_CREATION:
            creation_flow = CharacterCreation::advance(creation_flow, Constants::TIMESTEP);
            break;
        default:
            break;
        }

        switch (CharacterCreation::timeout_action(creation_flow, REQUEST_TIMEOUT))
        {
        case CharacterCreation::TimeoutAction::RESTORE_NAME_ENTRY:
            restore_name_entry();
            UI::get().emplace<UILoginNotice>(UILoginNotice::UNABLE_TO_CONNECT);
            break;
        case CharacterCreation::TimeoutAction::RESTORE_CUSTOMIZATION:
            restore_customization();
            UI::get().emplace<UILoginNotice>(UILoginNotice::UNABLE_TO_CONNECT);
            break;
        case CharacterCreation::TimeoutAction::RECHECK_CREATED_NAME:
        {
            std::string pending_name = creation_flow.pending_name;
            creation_flow = CharacterCreation::recovering_creation(pending_name);
            NameCharPacket(pending_name).dispatch();
            break;
        }
        default:
            break;
        }

        cloudfx += 0.25f;
    }

    const std::string& UICharcreation::get_equipname(Equipslot::Id slot) const
    {
        if (int32_t item_id = newchar.get_equips().get_equip(slot))
        {
            return ItemData::get(item_id)
                .get_name();
        }
        else
        {
            static const std::string& nullstr = "Missing name.";
            return nullstr;
        }
    }
}
