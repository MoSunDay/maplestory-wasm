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

#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    // Pure content model for the client-side quest offer dialog.
    //
    // Everything here is a free function without client state; only the
    // .cpp touches the quest nx tables. The Say.img speech was just
    // backfilled from the English XML and still carries the original
    // #b/#p/#t/#r/#k macro markers, so the pages are returned verbatim and
    // the dialog applies npctalk_text::replace_macros at layout time.
    namespace quest_dialog_model
    {
        // One quest interaction as offered by an NPC.
        struct DialogContent
        {
            // Speech pages shown in order, macro markers still intact.
            std::vector<std::string> pages;
            // NPC reply after declining; empty when Say.img ships none.
            std::string decline_page;
            // Quest name for the name plate.
            std::string title;
        };

        // Collects the dialogue of one quest interaction.
        //
        // completing == false reads Say.img/<qid>/0/* (accept speech) with
        // decline_page = Say.img/<qid>/0/no/0; completing == true reads the
        // "1" stage instead. Numbered children are ordered numerically and
        // only non-empty strings count. When every page is empty the
        // QuestInfo.img/<qid>/<0|1> summary paragraph is used, and when
        // that is missing too a Chinese fallback line keeps the dialog
        // usable. The title comes from QuestInfo.img/<qid>/name ("任务"
        // when absent).
        DialogContent collect(int16_t qid, bool completing);
    }
}
