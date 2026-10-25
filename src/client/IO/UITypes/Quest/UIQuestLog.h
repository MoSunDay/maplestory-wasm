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
#include "../../UIDragElement.h"

#include "../../Components/Slider.h"

#include "../../../Character/QuestLog.h"
#include "../../../Graphics/Geometry.h"
#include "../../../Graphics/Text.h"

#include "QuestLogModel.h"

#include <vector>

namespace jrc
{
    // The quest log window (Q key / statusbar menu): a left list pane with
    // an in-progress/completed tab pair and slider, and a right info pane
    // for the selected quest. Rows are rebuilt whenever the character's
    // quest log revision changes.
    class UIQuestLog : public UIDragElement<PosQUEST>
    {
    public:
        static constexpr Type TYPE = QUESTLOG;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        explicit UIQuestLog(const Questlog& quests);

        void draw(float alpha) const override;
        void update() override;
        bool remove_cursor(bool clicked, Point<int16_t> cursorpos) override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursorpos) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

        // Number of rows in the active filter (probe helper).
        int row_count() const;

    protected:
        Button::State button_pressed(uint16_t id) override;

    private:
        enum Buttons
        {
            BT_TAB0,
            BT_TAB1
        };

        static constexpr Point<int16_t> LIST_OFFSET = { 14, 70 };
        static constexpr int16_t TAB_Y = 12;
        static constexpr int16_t TAB_GAP = 2;

        void initialize_sprites();
        void initialize_tabs();
        void rebuild_rows(bool keep_selection);
        void refresh_info_texts();
        void change_filter(quest_log_model::Filter new_filter);
        void change_offset(int16_t new_offset);
        void select_row(int16_t row_index);

        const Questlog& quests;

        quest_log_model::Filter filter =
            quest_log_model::Filter::IN_PROGRESS;
        std::vector<quest_log_model::QuestRow> rows;
        std::vector<Text> row_labels;

        Slider slider;
        ColorBox selection_box;
        Text completed_label;
        Text tab0_label;
        Text tab1_label;
        Text selected_name;
        Text selected_category;
        Text selected_state;
        bool tabs_from_assets = true;

        int16_t list_width = 0;
        int16_t info_x = 0;
        int16_t offset = 0;
        int16_t selected = -1;
        uint64_t last_rev = 0;
    };
}
