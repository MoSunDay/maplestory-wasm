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
#include "../../../Data/ItemData.h"
#include "../../../Gameplay/Stage.h"
#include "../../../Graphics/GraphicsGL.h"
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

        // Choose-one reward block on the final page of a completing offer:
        // a flowing grid of icon cells below the speech text, plus the
        // hint line introducing it.
        constexpr int16_t REWARD_TEXT_GAP = 6;
        constexpr int16_t HINT_GAP = 4;
        constexpr int16_t REWARD_CELL_WIDTH = 64;
        constexpr int16_t REWARD_CELL_HEIGHT = 40;
        constexpr int16_t REWARD_ICON_WIDTH = 32;
        constexpr int16_t REWARDS_PER_ROW = TEXT_WIDTH / REWARD_CELL_WIDTH;

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
          rewards(quest_offer.completing
              ? quest_dialog_model::selectable_rewards(quest_offer.quest_id)
              : std::vector<quest_dialog_model::RewardChoice>()),
          reward_selection(0),
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
        // Selection page caption for prop == -1 completion rewards.
        hint = { Text::A12M, Text::LEFT, Text::DARKGREY,
                 "请选择一项奖励：", TEXT_WIDTH, false };

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

    bool UIQuestDialog::on_selection_page() const
    {
        // Only the completing flow renders the choose-one reward row;
        // accept offers carry no selectable Act.img rewards.
        return offer.completing && !rewards.empty() && on_last_page();
    }

    int16_t UIQuestDialog::rewards_height() const
    {
        if (!on_selection_page())
        {
            return 0;
        }

        const std::size_t rows =
            (rewards.size() + REWARDS_PER_ROW - 1) / REWARDS_PER_ROW;
        return static_cast<int16_t>(
            REWARD_TEXT_GAP + hint.height() + HINT_GAP
            + static_cast<int16_t>(rows) * REWARD_CELL_HEIGHT);
    }

    Point<int16_t> UIQuestDialog::rewards_origin() const
    {
        const int16_t block = static_cast<int16_t>(
            text.height() + rewards_height());
        const int16_t content_y = static_cast<int16_t>(
            DIALOG_TEXT_Y_OFFSET
                + ((vtile * fill.height() - block) / 2));
        return { DIALOG_TEXT_X,
            static_cast<int16_t>(content_y + text.height()
                + REWARD_TEXT_GAP) };
    }

    int16_t UIQuestDialog::reward_at(Point<int16_t> relative) const
    {
        if (!on_selection_page())
        {
            return -1;
        }

        // The grid starts under the hint line inside the reward block.
        const Point<int16_t> origin = rewards_origin()
            + Point<int16_t>(0,
                static_cast<int16_t>(hint.height() + HINT_GAP));
        for (std::size_t i = 0; i < rewards.size(); ++i)
        {
            const int16_t cell_x = static_cast<int16_t>(
                origin.x()
                + (i % REWARDS_PER_ROW) * REWARD_CELL_WIDTH);
            const int16_t cell_y = static_cast<int16_t>(
                origin.y()
                + (i / REWARDS_PER_ROW) * REWARD_CELL_HEIGHT);
            const Rectangle<int16_t> cell(
                cell_x, cell_x + REWARD_CELL_WIDTH,
                cell_y, cell_y + REWARD_CELL_HEIGHT);
            if (cell.contains(relative))
            {
                return static_cast<int16_t>(i);
            }
        }
        return -1;
    }

    void UIQuestDialog::show_page()
    {
        // The Say.img text carries #b/#p/#t/#r/#k marker tokens; STRIP drops
        // the sprite macros with everything else, leaving readable text.
        const std::string processed = npctalk_text::replace_macros(
            current_page(), npctalk_text::EmbedImages::STRIP);
        text = { Text::A12M, Text::LEFT, Text::DARKGREY, processed,
                 TEXT_WIDTH, false };

        // Fill-height math mirrored from UINpcTalk::change_text, plus the
        // choose-one reward block when the final page renders one.
        const int16_t minimum_fill_height =
            MIN_DIALOGUE_TILES * fill.height();
        const int16_t required_fill_height = std::max<int16_t>(
            minimum_fill_height,
            static_cast<int16_t>(
                text.height() + rewards_height()
                + TEXT_VERTICAL_PADDING * 2)
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
            else if (!rewards.empty())
            {
                // Selection reward: the trailing [short selection] indexes
                // the prop == -1 Act.img list; the dialog keeps the list
                // in the server's selectList order.
                QuestCompletePacket(offer.quest_id, npc_id, pos,
                    static_cast<int16_t>(reward_selection)).dispatch();
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

        const int16_t block = static_cast<int16_t>(
            text.height() + rewards_height());
        const int16_t text_y = static_cast<int16_t>(
            DIALOG_TEXT_Y_OFFSET
                + ((vtile * fill.height() - block) / 2));
        text.draw(position + Point<int16_t>(DIALOG_TEXT_X, text_y));

        // Choose-one reward row on the final page of a completing offer:
        // hint line, then a grid of icon cells (icon + xCount), the
        // selected cell highlighted.
        if (on_selection_page())
        {
            const Point<int16_t> origin = rewards_origin();
            hint.draw(position + origin);

            const int16_t grid_y = static_cast<int16_t>(
                origin.y() + hint.height() + HINT_GAP);
            for (std::size_t i = 0; i < rewards.size(); ++i)
            {
                const int16_t cell_x = static_cast<int16_t>(
                    origin.x()
                    + (i % REWARDS_PER_ROW) * REWARD_CELL_WIDTH);
                const int16_t cell_y = static_cast<int16_t>(
                    grid_y + (i / REWARDS_PER_ROW) * REWARD_CELL_HEIGHT);

                if (i == reward_selection)
                {
                    GraphicsGL::get().drawrectangle(
                        static_cast<int16_t>(position.x() + cell_x),
                        static_cast<int16_t>(position.y() + cell_y),
                        REWARD_CELL_WIDTH, REWARD_CELL_HEIGHT,
                        1.0f, 0.5f, 0.0f, 0.35f);
                }

                const ItemData& item = ItemData::get(rewards[i].itemid);
                item.get_icon(false).draw(
                    position + Point<int16_t>(
                        cell_x + (REWARD_CELL_WIDTH - REWARD_ICON_WIDTH) / 2,
                        cell_y));

                Text countlabel(Text::A11M, Text::RIGHT, Text::DARKGREY,
                    "x" + std::to_string(rewards[i].count));
                countlabel.draw(position + Point<int16_t>(
                    cell_x + REWARD_CELL_WIDTH - 2,
                    static_cast<int16_t>(cell_y + REWARD_CELL_HEIGHT
                        - countlabel.height() - 2)));
            }
        }
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

    UIElement::CursorResult UIQuestDialog::send_cursor(bool clicked,
        Point<int16_t> cursorpos)
    {
        if (on_selection_page())
        {
            const int16_t index = reward_at(cursorpos - position);
            if (index >= 0)
            {
                // Clicking a reward cell selects it (index 0 stays the
                // default); confirming still runs through the YES button.
                if (clicked)
                {
                    reward_selection = static_cast<std::size_t>(index);
                }
                return { clicked ? Cursor::CLICKING : Cursor::CANCLICK,
                    true };
            }
        }

        return UIElement::send_cursor(clicked, cursorpos);
    }

    int32_t UIQuestDialog::probe_button(int which, int field) const
    {
        if (which < 0 || which > 3)
        {
            return -9998;
        }
        const auto it = buttons.find(static_cast<uint16_t>(which));
        if (it == buttons.end())
        {
            return -9997;
        }
        const Button& button = *it->second;
        switch (field)
        {
        case 0:
            return button.get_position().x();
        case 1:
            return button.get_position().y();
        case 2:
            return button.width();
        case 3:
            return button.height();
        case 4:
            return button.is_active() ? 1 : 0;
        default:
            return -9996;
        }
    }

    int32_t UIQuestDialog::probe_element(int field) const
    {
        switch (field)
        {
        case 0:
            return position.x();
        case 1:
            return position.y();
        case 2:
            return dimension.x();
        case 3:
            return dimension.y();
        case 4:
            return on_last_page() ? 1 : 0;
        case 5:
            return static_cast<int32_t>(rewards.size());
        case 6:
            return rewards.empty()
                ? -1
                : static_cast<int32_t>(reward_selection);
        case 7:
            return static_cast<int32_t>(page_index);
        case 8:
            return declining ? 1 : 0;
        case 9:
            return offer.quest_id;
        case 10:
            return offer.completing ? 1 : 0;
        case 11:
            return offer.scripted ? 1 : 0;
        default:
            return -9996;
        }
    }

    int32_t UIQuestDialog::probe_reward(int index, int field) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= rewards.size())
        {
            return -9998;
        }
        if (!on_selection_page())
        {
            return -9997;
        }

        const Point<int16_t> grid = rewards_origin()
            + Point<int16_t>(0,
                static_cast<int16_t>(hint.height() + HINT_GAP));
        const int16_t cell_x = static_cast<int16_t>(
            grid.x() + (index % REWARDS_PER_ROW) * REWARD_CELL_WIDTH);
        const int16_t cell_y = static_cast<int16_t>(
            grid.y() + (index / REWARDS_PER_ROW) * REWARD_CELL_HEIGHT);
        switch (field)
        {
        case 0:
            return cell_x;
        case 1:
            return cell_y;
        case 2:
            return REWARD_CELL_WIDTH;
        case 3:
            return REWARD_CELL_HEIGHT;
        default:
            return -9996;
        }
    }
}
