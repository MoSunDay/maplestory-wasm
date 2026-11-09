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
#include "UIQuestDialog.h"

#include "../../UILayout.h"
#include "../../Components/MapleButton.h"

#include "../../../Console.h"
#include "../../../Constants.h"
#include "../../../Gameplay/Stage.h"
#include "../../../Net/Packets/QuestPackets.h"

#include "../UINpcTalkNodes.h"
#include "../UINpcTalkText.h"

#include "nlnx/node.hpp"

#include <algorithm>
#include <string>

namespace jrc
{
    namespace
    {
        // Layout constants copied from UINpcTalk so both dialogue windows
        // wrap identically inside the shared UtilDlgEx frame.
        constexpr int16_t MIN_DIALOGUE_TILES = 8;
        constexpr int16_t TEXT_WIDTH = 320;
        constexpr int16_t TEXT_VERTICAL_PADDING = 16;
        constexpr int16_t BUTTON_MARGIN = 20;
        constexpr int16_t BUTTON_GAP = 6;
        constexpr int16_t DIALOG_TEXT_X = 156;
        constexpr int16_t DIALOG_TEXT_Y_OFFSET = 16;

        // Name plate strip, mirroring the UINpcTalk speaker name row.
        constexpr Point<int16_t> NAMETAG_OFFSET = { 25, 100 };
        constexpr Point<int16_t> TITLE_OFFSET = { 80, 99 };
    }

    UIQuestDialog::UIQuestDialog(const QuestOffer& quest_offer,
        int32_t npc_template_id)
        : offer(quest_offer),
          npc_id(npc_template_id),
          content(quest_dialog_model::collect(
              quest_offer.quest_id, quest_offer.completing)),
          height(0),
          vtile(0),
          page_index(0),
          declining(false)
    {
        nl::node src = npctalk_nodes::src();

        top = src["t"];
        fill = src["c"];
        bottom = src["s"];
        nametag = src["bar"];

        buttons[NEXT] = std::make_unique<MapleButton>(src["BtNext"]);
        buttons[PREV] = std::make_unique<MapleButton>(src["BtPrev"]);
        buttons[YES] = std::make_unique<MapleButton>(src["BtYes"]);
        buttons[NO] = std::make_unique<MapleButton>(src["BtNo"]);

        // The quest name replaces the NPC name on the name plate.
        title = { Text::A11M, Text::CENTER, Text::WHITE, content.title };

        show_page();

        active = false;
    }

    const std::string& UIQuestDialog::current_page() const
    {
        if (declining)
        {
            return content.decline_page;
        }

        const std::size_t last =
            content.pages.empty() ? 0 : content.pages.size() - 1;
        return content.pages[std::min(page_index, last)];
    }

    bool UIQuestDialog::on_last_page() const
    {
        return !declining && page_index + 1 >= content.pages.size();
    }

    void UIQuestDialog::show_page()
    {
        // The Say.img text carries #b/#p/#t/#r/#k marker tokens; STRIP drops
        // the sprite macros with everything else, leaving readable text.
        const std::string processed = npctalk_text::replace_macros(
            current_page(), npctalk_text::EmbedImages::STRIP);
        text = { Text::A12M, Text::LEFT, Text::DARKGREY, processed,
                 TEXT_WIDTH, false };

        // Fill-height math mirrored from UINpcTalk::change_text.
        const int16_t minimum_fill_height =
            MIN_DIALOGUE_TILES * fill.height();
        const int16_t required_fill_height = std::max<int16_t>(
            minimum_fill_height,
            static_cast<int16_t>(
                text.height() + TEXT_VERTICAL_PADDING * 2)
        );
        vtile = std::max<int16_t>(
            MIN_DIALOGUE_TILES,
            static_cast<int16_t>(
                (required_fill_height + fill.height() - 1) / fill.height())
        );

        // Cap the dialog so it stays on screen next to the frame chrome.
        const int16_t max_fill = static_cast<int16_t>(
            (Constants::viewheight() - top.height() - bottom.height() - 40)
                / fill.height());
        if (max_fill > MIN_DIALOGUE_TILES && vtile > max_fill)
        {
            vtile = max_fill;
        }

        height = vtile * fill.height();

        dimension = { top.width(), static_cast<int16_t>(
            top.height() + height + bottom.height()) };
        position = ui_layout::centered_box(dimension.x(), dimension.y());

        for (auto& button : buttons)
        {
            button.second->set_active(false);
            button.second->set_state(Button::NORMAL);
        }

        auto place_button = [&](Buttons id, int16_t x) {
            int16_t footer_y = top.height() + height;
            int16_t button_y = footer_y
                + std::max<int16_t>(
                    0, (bottom.height() - buttons[id]->height()) / 2);
            buttons[id]->set_position({ x, button_y });
            buttons[id]->set_active(true);
        };

        int16_t right_edge = top.width() - BUTTON_MARGIN;
        auto place_button_from_right = [&](Buttons id) {
            right_edge -= buttons[id]->width();
            place_button(id, right_edge);
            right_edge -= BUTTON_GAP;
        };

        if (declining)
        {
            // Decline reply page: one confirm button only. Kept simple by
            // re-using the YES button (right-most slot) and closing on
            // press without sending anything.
            place_button_from_right(YES);
        }
        else if (on_last_page())
        {
            place_button_from_right(NO);
            place_button_from_right(YES);
        }
        else
        {
            if (page_index > 0)
            {
                place_button_from_right(PREV);
            }
            place_button_from_right(NEXT);
        }
    }

    void UIQuestDialog::dispatch_accept()
    {
        Point<int16_t> pos = Stage::get().get_player().get_position();

        if (offer.completing)
        {
            if (offer.scripted)
            {
                QuestScriptEndPacket(offer.quest_id, npc_id, pos).dispatch();
            }
            else
            {
                QuestCompletePacket(offer.quest_id, npc_id, pos).dispatch();
            }
        }
        else if (offer.scripted)
        {
            QuestScriptStartPacket(offer.quest_id, npc_id, pos).dispatch();
        }
        else
        {
            QuestStartPacket(offer.quest_id, npc_id, pos).dispatch();
        }

        Console::get().print(
            "[quest] npc " + std::to_string(npc_id)
            + (offer.completing ? " 交付 " : " 接取 ")
            + (offer.scripted ? "脚本任务 " : "任务 ")
            + std::to_string(offer.quest_id));
    }

    void UIQuestDialog::draw(float inter) const
    {
        Point<int16_t> drawpos = position;
        top.draw(drawpos);
        drawpos.shift_y(top.height());
        fill.draw({ drawpos, Point<int16_t>(0, vtile) * fill.height() });
        drawpos.shift_y(vtile * fill.height());
        bottom.draw(drawpos);

        UIElement::draw(inter);

        nametag.draw(position + NAMETAG_OFFSET);
        title.draw(position + TITLE_OFFSET);

        const int16_t text_y = static_cast<int16_t>(
            DIALOG_TEXT_Y_OFFSET
                + ((vtile * fill.height() - text.height()) / 2));
        text.draw(position + Point<int16_t>(DIALOG_TEXT_X, text_y));
    }

    void UIQuestDialog::update_screen(int16_t, int16_t)
    {
        // Re-center the laid-out dialog on the shared design canvas; the
        // wrap only depends on the dialog width and the buttons are stored
        // relative to `position` (mirrors UINpcTalk::update_screen).
        position = ui_layout::centered_box(dimension.x(), dimension.y());
    }

    Button::State UIQuestDialog::button_pressed(uint16_t buttonid)
    {
        switch (buttonid)
        {
        case NEXT:
            if (!declining && !on_last_page())
            {
                ++page_index;
                show_page();
            }
            break;
        case PREV:
            if (!declining && page_index > 0)
            {
                --page_index;
                show_page();
            }
            break;
        case YES:
            if (declining)
            {
                // Confirm on the decline reply page: close only.
                active = false;
            }
            else if (on_last_page())
            {
                dispatch_accept();
                active = false;
            }
            break;
        case NO:
            if (declining)
            {
                active = false;
            }
            else if (on_last_page())
            {
                if (!content.decline_page.empty())
                {
                    declining = true;
                    page_index = 0;
                    show_page();
                }
                else
                {
                    active = false;
                }
            }
            break;
        }
        return Button::PRESSED;
    }

    void UIQuestDialog::send_key(int32_t, bool pressed, bool escape)
    {
        if (!pressed || !escape)
        {
            return;
        }

        // ESC closes the offer without sending any packet.
        active = false;
    }
}
