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
#pragma once
#include "../../UIElement.h"

#include "../../../Character/QuestCheck.h"
#include "../../../Graphics/Text.h"
#include "../../../Graphics/Texture.h"

#include "QuestDialogModel.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jrc
{
    // Client-side quest offer dialog, opened by quest_interaction when a
    // TalkToNPC went unanswered by the server. Visuals and lifecycle
    // mirror UINpcTalk: the same UtilDlgEx frame (top/fill/bottom tiles,
    // name plate, BtNext/BtPrev/BtYes/BtNo buttons), the same wrap width
    // and fill-height math.
    //
    // Page flow: NEXT/PREV walk the Say.img speech pages, the last page
    // swaps them for YES/NO. YES dispatches the quest action packet for
    // the offer; NO shows the decline reply page (when Say.img has one)
    // whose single confirm button just closes. ESC/END close without
    // sending anything.
    class UIQuestDialog : public UIElement
    {
    public:
        static constexpr Type TYPE = QUESTDIALOG;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        UIQuestDialog(const QuestOffer& offer, int32_t npc_id);

        void draw(float inter) const override;
        void update_screen(int16_t width, int16_t height) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;
        UIElement::CursorResult send_cursor(bool clicked,
            Point<int16_t> cursorpos) override;

        // 只读诊断探针（ImeBridge 调用）：which 0=NEXT 1=PREV 2=YES 3=NO；
        // field 0=x 1=y 2=w 3=h 4=active。元素级 field:
        // 0=pos.x 1=pos.y 2=dim.x 3=dim.y 4=on_last_page
        // 5=selection_count 6=selected_index 7=page_index 8=declining
        // 9=quest_id 10=completing 11=scripted。
        int32_t probe_button(int which, int field) const;
        int32_t probe_element(int field) const;
        // 奖励格：index 为 rewards 下标，field 0=x 1=y 2=w 3=h
        // （均为对话框相对坐标），越界返回 -9998。
        int32_t probe_reward(int index, int field) const;

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        enum Buttons
        {
            NEXT,
            PREV,
            YES,
            NO
        };

        // Rebuilds text and button visibility for the current page (or the
        // decline reply page) and re-runs the fill-height layout.
        void show_page();
        // Raw text of the page currently on screen.
        const std::string& current_page() const;
        // Whether the last speech page (the YES/NO page) is on screen.
        bool on_last_page() const;
        // Whether the final page also renders the choose-one reward row
        // (completion offer whose Act.img table has prop == -1 items).
        bool on_selection_page() const;
        // Height of the reward row block, 0 when it is not shown.
        int16_t rewards_height() const;
        // Top-left corner of the reward block, relative to `position`.
        Point<int16_t> rewards_origin() const;
        // Index of the reward cell under the dialog-relative cursor, -1
        // when the cursor is outside every cell.
        int16_t reward_at(Point<int16_t> relative) const;
        // Sends the quest action packet matching offer.completing/scripted.
        void dispatch_accept();

        QuestOffer offer;
        int32_t npc_id;
        quest_dialog_model::DialogContent content;

        // prop == -1 completion rewards (Act.img/<qid>/1/item); empty for
        // quests without a choose-one reward. `reward_selection` is the
        // index sent as the trailing [short selection] byte pair.
        std::vector<quest_dialog_model::RewardChoice> rewards;
        std::size_t reward_selection;

        Texture top;
        Texture fill;
        Texture bottom;
        Texture nametag;

        Text title;
        Text text;
        Text hint;
        int16_t height;
        int16_t vtile;

        std::size_t page_index;
        bool declining;
    };
}
