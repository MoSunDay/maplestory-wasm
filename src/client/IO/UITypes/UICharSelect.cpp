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
#include "../UILayout.h"
#include "UICharSelectNodes.h"

#include "UISoftKey.h"
#include "UICharCreation.h"
#include "UILoginNotice.h"

#include "../UI.h"

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
        : bitmap_ui(false), info_has_rank_rows(false),
          characters(std::move(cs)), require_pic(p), channel_id(ch),
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

        position  = ui_layout::origin();
        dimension = { 800, 600 };
        active    = true;
    }

    void UICharSelect::update_screen(int16_t, int16_t)
    {
        position = ui_layout::origin();
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

        update_bitmap_values();
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
