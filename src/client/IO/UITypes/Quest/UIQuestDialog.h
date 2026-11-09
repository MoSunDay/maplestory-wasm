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
        // Sends the quest action packet matching offer.completing/scripted.
        void dispatch_accept();

        QuestOffer offer;
        int32_t npc_id;
        quest_dialog_model::DialogContent content;

        Texture top;
        Texture fill;
        Texture bottom;
        Texture nametag;

        Text title;
        Text text;
        int16_t height;
        int16_t vtile;

        std::size_t page_index;
        bool declining;
    };
}
