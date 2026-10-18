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
#include "UICharSelect.h"
#include "../UiAssetVersion.h"
#include "UICharSelectNodes.h"

#include "UISoftKey.h"
#include "UICharCreation.h"
#include "UILoginNotice.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/AreaButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Constants.h"
#include "../../Configuration.h"
#include "../../Audio/Audio.h"
#include "../../Console.h"
#include "../../Character/Job.h"
#include "../../Net/Packets/SelectCharPackets.h"
#include "../../Net/Login/CharlistTransition.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace
    {
        constexpr const char* AUTO_PIC = "1010";
    }

    UICharSelect::UICharSelect(std::vector<CharEntry> cs,
                               uint8_t s,
                               uint8_t ch,
                               int8_t p)
        : characters(std::move(cs)), require_pic(p), channel_id(ch),
          pending_pic_action(PicAction::NONE), pending_pic_cid(-1),
          preview_ready(0), preview_completion_announced(false),
          ui_initialized(false), ui_failed(false),
          charcount_absolute(static_cast<uint8_t>(characters.size())),
          charcount_relative(0), slots_absolute(s), slots_relative(0),
          selected_absolute(0), selected_relative(0), page(0)
    {
        selected_absolute = Setting<DefaultCharacter>::get().load();
        selected_relative = selected_absolute % PAGESIZE;
        page              = selected_absolute / PAGESIZE;

        charlooks.resize(characters.size());
        preview_draw_failed.resize(characters.size(), false);
        preview_states.resize(
            characters.size(),
            character_preview::LoadState::PENDING
        );

        position  = { 0, 0 };
        dimension = { 800, 600 };
        active    = true;
    }

    void UICharSelect::initialize_ui()
    {

        nl::node back       = nl::nx::map["Back"]["login.img"]["back"];
        nl::node title      = nl::nx::ui["Login.img"]["Title"];
        nl::node common     = nl::nx::ui["Login.img"]["Common"];
        nl::node charselect = nl::nx::ui["Login.img"]["CharSelect"];

        // Compatibility: base login background exists across more UI.nx variants
        // than Title/worldsel.
        if (back["11"])
        {
            sprites.emplace_back(back["11"], Point<int16_t>(360, 313));
        }
        else if (back["0"])
        {
            sprites.emplace_back(back["0"], Point<int16_t>(370, 300));
        }

        // Keep legacy world-select art as an optional overlay when available.
        if (title["worldsel"])
        {
            sprites.emplace_back(title["worldsel"]);
        }

        if (common["frame"])
        {
            sprites.emplace_back(common["frame"], Point<int16_t>(400, login_frame_y()));
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/Common/frame for this UI.nx variant.");
        }

        // Post BB
        /*
        selworldpos = Point<int16_t>(578, 42);
        charinfopos = Point<int16_t>(662, 355);
        buttons[BT_ARBEIT] = std::make_unique<MapleButton>(charsel["arbeit"], Point<int16_t>(580, 115)));
        buttons[BT_CARDS] = std::make_unique<MapleButton>(charsel["characterCard"], Point<int16_t>(665, 115)));
        */

        // Pre BB
        // Post-Chaos keeps the upstream anchor. The pre-Chaos board
        // (charInfo1, 183x151, origin (45,57)) would run flush against
        // the right screen edge there, so it is shifted left.
        const bool post_chaos_assets = charselect_nodes::post_chaos();
        charinfopos = post_chaos_assets ? Point<int16_t>(662, 305)
                                        : Point<int16_t>(625, 305);
        selworldpos = Point<int16_t>(578, 112);

        sprites.emplace_back(charselect_nodes::info_board(),      charinfopos);
        sprites.emplace_back(common["selectWorld"],               selworldpos);

        if (charselect["selectedWorld"])
        {
            // Post-Chaos world banner art. Pre-Chaos only ships the
            // Common/selectWorld board added above.
            sprites.emplace_back(charselect["selectedWorld"]["icon"]["15"],     selworldpos);
            sprites.emplace_back(charselect["selectedWorld"]["name"]["15"],     selworldpos);
            sprites.emplace_back(charselect["selectedWorld"]["ch"][channel_id], selworldpos);
        }

        if (charselect["charSlot"])
        {
            nl::node charslot = charselect["charSlot"];
            Point<int16_t> charslotpos = charslot["pos"];
            sprites.emplace_back(charslot["0"], charslotpos);
            slotcountpos = charslotpos + Point<int16_t>(charslot["numberpos"]);
        }
        else
        {
            // Pre-Chaos has no slot window: anchor the count label at the
            // bottom right corner of the selectWorld board instead.
            slotcountpos = selworldpos + Point<int16_t>(144, 71);
        }
        slotcountlabel = { Text::A11M, Text::RIGHT, Text::WHITE };

        emptyslot    = charselect_nodes::empty_slot();
        charplatform = charselect_nodes::platform();
        nametag      = charselect["nameTag"];

        buttons[BT_SELECTCHAR] = std::make_unique<MapleButton>(
            charselect["BtSelect"],
            post_chaos_assets ? charinfopos + Point<int16_t>(-76, 72)
                              : Point<int16_t>(440, 495));
        buttons[BT_CREATECHAR] = std::make_unique<MapleButton>(charselect["BtNew"],    Point<int16_t>(200, 495));
        buttons[BT_DELETECHAR] = std::make_unique<MapleButton>(charselect["BtDelete"], Point<int16_t>(320, 495));

        if (charselect_nodes::pages_are_state_buttons())
        {
            buttons[BT_PAGELEFT]   = std::make_unique<MapleButton>(charselect["pageL"],    charselect_layout::PAGE_ARROW_LEFT_POS);
            buttons[BT_PAGERIGHT]  = std::make_unique<MapleButton>(charselect["pageR"],    charselect_layout::PAGE_ARROW_RIGHT_POS);
        }
        else
        {
            // Pre-Chaos pageL/pageR are big edge arrows stored as plain
            // frame pairs instead of state subtrees. Their origins are
            // (43,37)/(44,36), so the anchors keep the upstream top left
            // corners and the right arrow mirrors the left one at the
            // opposite screen edge.
            buttons[BT_PAGELEFT]   = std::make_unique<TwoSpriteButton>(
                charselect["pageL"]["1"], charselect["pageL"]["0"],
                Point<int16_t>(100, 490));
            buttons[BT_PAGERIGHT]  = std::make_unique<TwoSpriteButton>(
                charselect["pageR"]["1"], charselect["pageR"]["0"],
                Point<int16_t>(698, 490));
        }

        for (uint8_t i = 0; i < PAGESIZE; ++i)
        {
            buttons[BT_CHAR0 + i] = std::make_unique<AreaButton>(
                charselect_layout::slot_hotzone_pos(i),
                charselect_layout::slot_hotzone_dim()
            );
        }

        levelset = { charselect["lv"], Charset::CENTER };

        namelabel = { Text::A18M, Text::CENTER };
        for (size_t i = 0; i < NUM_LABELS; ++i)
        {
            infolabels[i] = { Text::A11M, Text::RIGHT };
        }

        nametags.reserve(characters.size());
        for (const auto& entry : characters)
        {
            nametags.emplace_back(nametag, Text::A13M, Text::WHITE, entry.stats.name);
        }

        update_counts();
        update_selection();
    }

    void UICharSelect::draw(float alpha) const
    {
        UIElement::draw(alpha);
        if (!ui_initialized)
        {
            return;
        }

        const bool post_chaos_assets = charselect_nodes::post_chaos();
        slotcountlabel.draw(slotcountpos);

        for (uint8_t i = 0; i < charcount_relative; ++i)
        {
            const Point<int16_t> charpos = charselect_layout::char_slot_pos(i);
            const uint8_t index = i + page * PAGESIZE;
            // The post-Chaos disc origin sinks the feet; the pre-Chaos
            // platform already sits correctly at the feet anchor.
            const Point<int16_t> platformpos = post_chaos_assets
                ? charpos + charselect_layout::PLATFORM_OFFSET
                : charpos;
            charplatform.draw(platformpos, alpha);
            draw_preview(index, charpos, alpha);
            nametags[index].draw(
                post_chaos_assets
                    ? charpos + charselect_layout::NAMETAG_OFFSET
                    : charpos
            );
        }

        if (selected_absolute < charcount_absolute)
        {
            const StatsEntry& stats = characters[selected_absolute].stats;
            const Point<int16_t> data_origin = charinfopos + charselect_layout::CHARACTER_DATA_OFFSET;

            std::string levelstr = std::to_string(stats.stats[Maplestat::LEVEL]);
            if (charselect_nodes::has_level_charset())
            {
                int16_t lvx = levelset.draw(levelstr, charinfopos + Point<int16_t>(23, -93));
                levelset.draw('l', charinfopos + Point<int16_t>(-7 - lvx / 2, -93));

                namelabel.draw(charinfopos + Point<int16_t>(0, -85));
            }
            else
            {
                // Pre-Chaos ships no lv charset, so the marker is a plain
                // label. The info board top edge sits 57px above the
                // anchor; the level goes on the first board row and the
                // name right below it.
                Text& lvlabel = charselect_nodes::level_label();
                lvlabel.change_text("Lv. " + levelstr);
                lvlabel.draw(charinfopos + Point<int16_t>(46, -37));

                namelabel.draw(charinfopos + Point<int16_t>(46, -13));
            }

            for (size_t i = 0; i < NUM_LABELS; ++i)
            {
                Point<int16_t> labelpos = data_origin + charselect_layout::label_pos(i, post_chaos_assets);
                infolabels[i].draw(labelpos);
            }
        }

        for (uint8_t i = charcount_relative; i < slots_relative; ++i)
        {
            const Point<int16_t> position_slot = charselect_layout::char_slot_pos(i);
            if (post_chaos_assets)
            {
                charplatform.draw(position_slot + charselect_layout::PLATFORM_OFFSET, alpha);
                emptyslot.draw(position_slot, alpha);
            }
            else
            {
                // Pre-Chaos keeps the empty platform under unused slots
                // and puts the small icon/new badge just above it.
                charplatform.draw(position_slot, alpha);
                emptyslot.draw(position_slot + Point<int16_t>(-4, -84), alpha);
            }
        }
    }

    void UICharSelect::update()
    {
        UIElement::update();

        if (ui_failed)
        {
            return;
        }

        try
        {
            if (!ui_initialized)
            {
                charlist_transition::set_phase(
                    charlist_transition::Phase::STATIC_UI_LOADING
                );
                initialize_ui();
                ui_initialized = true;
                charlist_transition::set_phase(
                    charlist_transition::Phase::PREVIEW_LOADING
                );
            }

            load_next_preview();
        }
        catch (const std::exception& error)
        {
            fail_ui(error);
            return;
        }
        catch (...)
        {
            fail_ui("unknown exception");
            return;
        }

        emptyslot.update();
        charplatform.update();

        for (size_t index = 0; index < charlooks.size(); ++index)
        {
            if (!charlooks[index])
            {
                continue;
            }

            try
            {
                charlooks[index]->update(Constants::TIMESTEP);
            }
            catch (const std::exception& error)
            {
                fail_preview(index, error.what());
            }
            catch (...)
            {
                fail_preview(index, "unknown exception during update");
            }
        }
    }

    Button::State UICharSelect::button_pressed(uint16_t bid)
    {
        if (!ui_initialized || ui_failed)
        {
            return Button::DISABLED;
        }

        if (bid == 9999)
            return Button::DISABLED;
        if (bid >= BT_CHAR0 && bid < BT_CHAR0 + PAGESIZE)
        {
            const uint8_t next_relative = static_cast<uint8_t>(bid - BT_CHAR0);
            if (next_relative >= charcount_relative ||
                selected_absolute >= nametags.size())
            {
                return Button::DISABLED;
            }

            nametags[selected_absolute].set_selected(false);
            if (selected_absolute < charlooks.size() &&
                charlooks[selected_absolute])
            {
                charlooks[selected_absolute]->set_stance(Stance::STAND1);
            }
            buttons[BT_CHAR0 + selected_relative]->set_state(Button::NORMAL);

            selected_relative = next_relative;
            selected_absolute = selected_relative + page * PAGESIZE;
            update_selection();

            return Button::IDENTITY;
        }
        else
        {
            switch (bid)
            {
            case BT_SELECTCHAR:
                send_selection();
                return Button::NORMAL;
            case BT_CREATECHAR:
                active = false;
                UI::get().emplace<UICharcreation>();
                return Button::NORMAL;
            case BT_DELETECHAR:
                send_deletion();
                return Button::PRESSED;
            case BT_PAGERIGHT:
                page++;
                update_counts();
                update_selection();
                return Button::IDENTITY;
            case BT_PAGELEFT:
                page--;
                update_counts();
                update_selection();
                return Button::IDENTITY;
            default:
                return Button::PRESSED;
            }
        }
    }

    void UICharSelect::doubleclick(Point<int16_t> pos)
    {
        if (!ui_initialized || ui_failed)
        {
            return;
        }

        for (uint8_t i = 0; i < PAGESIZE; ++i)
        {
            const uint8_t next_relative = i;
            if (next_relative >= charcount_relative)
            {
                break;
            }

            if (buttons[BT_CHAR0 + i]->bounds(position).contains(pos))
            {
                nametags[selected_absolute].set_selected(false);
                buttons[BT_CHAR0 + selected_relative]->set_state(Button::NORMAL);
                selected_relative = next_relative;
                selected_absolute = next_relative + page * PAGESIZE;
                update_selection();
                send_selection();
                return;
            }
        }
    }

    void UICharSelect::update_selection()
    {
        if (selected_absolute >= charcount_absolute || selected_relative >= charcount_relative)
        {
            return;
        }

        if (selected_absolute >= characters.size() ||
            selected_absolute >= charlooks.size() ||
            selected_absolute >= nametags.size())
        {
            return;
        }

        if (charlooks[selected_absolute])
        {
            charlooks[selected_absolute]->set_stance(Stance::STAND1);
        }
        nametags[selected_absolute].set_selected(true);

        buttons[BT_CHAR0 + selected_relative]->set_state(Button::PRESSED);
        namelabel.change_text(characters[selected_absolute].stats.name);

        for (size_t i = 0; i < NUM_LABELS; ++i)
        {
            infolabels[i].change_text(get_label_string(i));
        }
    }

    void UICharSelect::update_counts()
    {
        if (page > 0)
        {
            buttons[BT_PAGELEFT]->set_state(Button::NORMAL);
        }
        else
        {
            buttons[BT_PAGELEFT]->set_state(Button::DISABLED);
        }

        if (page < slots_absolute / PAGESIZE)
        {
            buttons[BT_PAGERIGHT]->set_state(Button::NORMAL);
        }
        else
        {
            buttons[BT_PAGERIGHT]->set_state(Button::DISABLED);
        }

        charcount_relative = charcount_absolute;
        if (charcount_relative > (page + 1) * PAGESIZE)
        {
            charcount_relative = PAGESIZE;
        }
        else if (charcount_relative < page * PAGESIZE)
        {
            charcount_relative = 0;
        }
        else
        {
            charcount_relative -= page * PAGESIZE;
        }

        if (selected_absolute >= charcount_absolute)
        {
            selected_absolute = 0;
        }

        selected_relative = selected_absolute % PAGESIZE;
        if (selected_relative >= charcount_relative)
        {
            selected_relative = 0;
        }

        slots_relative = slots_absolute - page * PAGESIZE;
        if (slots_relative > PAGESIZE)
        {
            slots_relative = PAGESIZE;
        }

        // Enablement is account-wide, not per page: a full single
        // page must still allow creating characters in free slots.
        if (charcount_absolute < slots_absolute)
        {
            buttons[BT_CREATECHAR]->set_state(Button::NORMAL);
        }
        else
        {
            buttons[BT_CREATECHAR]->set_state(Button::DISABLED);
        }

        slotcountlabel.change_text(
            std::to_string(charcount_absolute) + "/" + std::to_string(slots_absolute)
        );

        if (charcount_relative > 0)
        {
            buttons[BT_DELETECHAR]->set_state(Button::NORMAL);
        }
        else
        {
            buttons[BT_DELETECHAR]->set_state(Button::DISABLED);
        }

        for (uint8_t i = 0; i < PAGESIZE; ++i)
        {
            if (i < charcount_relative)
            {
                buttons[BT_CHAR0 + i]->set_state(Button::NORMAL);
            }
            else
            {
                buttons[BT_CHAR0 + i]->set_state(Button::DISABLED);
            }
        }
    }

    void UICharSelect::dispatch_pic_action(PicAction action, int32_t cid, const std::string& pic, bool auto_submit)
    {
        UI::get().disable();

        if (auto_submit)
        {
            pending_pic_action = action;
            pending_pic_cid = cid;
        }
        else
        {
            clear_pending_pic_request();
        }

        switch (action)
        {
        case PicAction::REGISTER:
            Console::get().print((auto_submit ? "Registering" : "Retrying registration with") + std::string(" PIC ") + pic);
            RegisterPicPacket(cid, pic).dispatch();
            break;
        case PicAction::SELECT:
            Console::get().print((auto_submit ? "Selecting character with" : "Retrying character select with") + std::string(" PIC ") + pic);
            SelectCharPicPacket(pic, cid).dispatch();
            break;
        case PicAction::DELETE:
            Console::get().print((auto_submit ? "Deleting character with" : "Retrying deletion with") + std::string(" PIC ") + pic);
            DeleteCharPacket(pic, cid).dispatch();
            break;
        case PicAction::NONE:
            clear_pending_pic_request();
            UI::get().enable();
            break;
        }
    }

    void UICharSelect::prompt_pic_entry(PicAction action, int32_t cid)
    {
        clear_pending_pic_request();
        UI::get().emplace<UISoftkey>([this, action, cid](const std::string& pic) {
            dispatch_pic_action(action, cid, pic, false);
        });
    }

    bool UICharSelect::handle_pic_failure(int8_t message, int32_t cid)
    {
        if (pending_pic_action == PicAction::NONE)
        {
            return false;
        }

        if (cid != -1 && pending_pic_cid != cid)
        {
            return false;
        }

        switch (message)
        {
        case UILoginNotice::SECOND_PASSWORD_INCORRECT:
        case UILoginNotice::SECOND_PASSWORD_NOT_DIFFERENT:
            Console::get().print("Auto PIC was rejected. Prompting for manual PIC entry.");
            prompt_pic_entry(pending_pic_action, pending_pic_cid);
            return true;
        default:
            clear_pending_pic_request();
            return false;
        }
    }

    void UICharSelect::clear_pending_pic_request()
    {
        pending_pic_action = PicAction::NONE;
        pending_pic_cid = -1;
    }

    void UICharSelect::send_selection()
    {
        if (selected_relative >= charcount_relative)
        {
            buttons[BT_SELECTCHAR]->set_state(Button::MOUSEOVER);
            return;
        }

        Setting<DefaultCharacter>::get().save(selected_absolute);
        int32_t cid = characters[selected_absolute].cid;
        switch (require_pic)
        {
        case 0:
            dispatch_pic_action(PicAction::REGISTER, cid, AUTO_PIC, true);
            break;
        case 1:
            dispatch_pic_action(PicAction::SELECT, cid, AUTO_PIC, true);
            break;
        case 2:
            clear_pending_pic_request();
            UI::get().disable();
            Sound(Sound::SELECTCHAR).play();
            SelectCharPacket(cid).dispatch();
            break;
        default:
            Console::get().print("require_pic = " + std::to_string(require_pic));
            break;
        }
    }

    void UICharSelect::send_deletion()
    {
        if (selected_relative >= charcount_relative)
        {
            buttons[BT_DELETECHAR]->set_state(Button::MOUSEOVER);
            return;
        }

        int32_t cid = characters[selected_absolute].cid;
        dispatch_pic_action(PicAction::DELETE, cid, AUTO_PIC, true);
    }

    void UICharSelect::add_character(CharEntry&& character)
    {
        characters.emplace_back(std::forward<CharEntry>(character));
        const CharEntry& created_character = characters.back();
        charlooks.emplace_back(std::nullopt);
        preview_states.emplace_back(character_preview::LoadState::PENDING);
        preview_draw_failed.push_back(false);
        preview_completion_announced = false;
        UI::get().disable();
        nametags.emplace_back(nametag, Text::A13M, Text::WHITE, created_character.stats.name);

        if (selected_absolute < charlooks.size() - 1)
        {
            if (charlooks[selected_absolute])
            {
                charlooks[selected_absolute]->set_stance(Stance::STAND1);
            }
            nametags[selected_absolute].set_selected(false);

            if (selected_relative < PAGESIZE)
            {
                buttons[BT_CHAR0 + selected_relative]->set_state(Button::NORMAL);
            }
        }

        charcount_absolute++;
        selected_absolute = charcount_absolute - 1;
        page = selected_absolute / PAGESIZE;
        selected_relative = selected_absolute % PAGESIZE;

        update_counts();
        update_selection();
        charlist_transition::set_preview_progress(preview_ready, charlooks.size());
        charlist_transition::set_phase(charlist_transition::Phase::PREVIEW_LOADING);
    }

    void UICharSelect::remove_char(int32_t cid)
    {
        size_t index = 0;
        for (const auto& character : characters)
        {
            if (character.cid == cid)
            {
                break;
            }

            index++;
        }

        if (index == characters.size())
        {
            return;
        }

        characters.erase(characters.begin() + index);
        charlooks.erase(charlooks.begin() + index);
        preview_states.erase(preview_states.begin() + index);
        preview_draw_failed.erase(preview_draw_failed.begin() + index);
        nametags.erase(nametags.begin() + index);

        charcount_absolute--;
        preview_ready = character_preview::resolved_count(preview_states);
        update_counts();
        update_selection();
        charlist_transition::set_preview_progress(preview_ready, charlooks.size());
        complete_previews_if_ready();
    }

    const CharEntry& UICharSelect::get_character(int32_t cid)
    {
        for (const auto& character : characters)
        {
            if (character.cid == cid)
            {
                return character;
            }
        }

        Console::get().print(
            __func__,
            "Warning: Invalid cid (" + std::to_string(cid) + ")"
        );

        static const CharEntry null_entry{ {}, {}, 0 };
        return null_entry;
    }

    std::string UICharSelect::get_label_string(size_t label) const
    {
        const StatsEntry& stats = characters[selected_absolute].stats;
        switch (label)
        {
        case JOB:
            return Job(stats.stats[Maplestat::JOB]).get_name();
        case STR:
            return std::to_string(stats.stats[Maplestat::STR]);
        case DEX:
            return std::to_string(stats.stats[Maplestat::DEX]);
        case INT:
            return std::to_string(stats.stats[Maplestat::INT]);
        case LUK:
            return std::to_string(stats.stats[Maplestat::LUK]);
        default:
            return "";
        }
    }
}
