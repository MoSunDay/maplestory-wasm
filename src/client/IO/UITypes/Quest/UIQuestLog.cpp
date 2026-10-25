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
#include "UIQuestLog.h"

#include "../../Components/TwoSpriteButton.h"
#include "QuestLogNodes.h"

#include "nlnx/node.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        using Filter = quest_log_model::Filter;

        constexpr int16_t PAGE_ROWS =
            static_cast<int16_t>(quest_log_model::ROWS_PER_PAGE);
        constexpr int16_t TAB_X = 10;
        constexpr int16_t SLIDER_MARGIN = 12;
        constexpr int16_t COMPLETED_LABEL_Y = 366;
        constexpr Point<int16_t> INFO_NAME_OFFSET = { 20, 44 };
        constexpr Point<int16_t> INFO_CATEGORY_OFFSET = { 20, 68 };
        constexpr Point<int16_t> INFO_STATE_OFFSET = { 20, 90 };
    }

    constexpr Point<int16_t> UIQuestLog::LIST_OFFSET;

    UIQuestLog::UIQuestLog(const Questlog& quest_log)
        : UIDragElement({ 250, 20 }),
          quests(quest_log),
          tab0_label(Text::A11M, Text::LEFT, Text::WHITE, "In Progress"),
          tab1_label(Text::A11M, Text::LEFT, Text::LIGHTGREY, "Completed")
    {
        initialize_sprites();
        initialize_tabs();

        slider = {
            11,
            {
                LIST_OFFSET.y(),
                static_cast<int16_t>(LIST_OFFSET.y() + PAGE_ROWS * quest_log_model::ROW_HEIGHT)
            },
            static_cast<int16_t>(list_width - SLIDER_MARGIN),
            PAGE_ROWS,
            1,
            [&](bool upwards)
            {
                change_offset(static_cast<int16_t>(offset + (upwards ? -1 : 1)));
            }
        };

        last_rev = quests.revision();
        rebuild_rows(false);

        if (tabs_from_assets)
        {
            buttons[BT_TAB0]->set_state(Button::PRESSED);
        }
    }

    void UIQuestLog::initialize_sprites()
    {
        const nl::node list = questlog_nodes::list_src();
        const nl::node info = questlog_nodes::info_src();
        const bool chaos = questlog_nodes::post_chaos();

        const Texture list_bg(list["backgrnd"]);
        list_width = list_bg.get_dimensions().x();
        sprites.emplace_back(list["backgrnd"]);

        // The post-Chaos list pane ships a second inner shade; the v83 pane
        // draws its whole furniture in one texture.
        if (chaos)
        {
            sprites.emplace_back(list["backgrnd2"]);
        }

        info_x = list_width;
        // v83 keeps the info pane as backgrnd3 inside the same subtree,
        // post-Chaos moves it to quest_info/backgrnd.
        const nl::node info_bg = chaos ? info["backgrnd"] : info["backgrnd3"];
        const Point<int16_t> info_position(info_x, 0);
        sprites.emplace_back(info_bg, info_position);
        if (chaos)
        {
            sprites.emplace_back(info["backgrnd2"], info_position);
        }

        const int16_t total_width = list_width + Texture(info_bg).get_dimensions().x();
        const int16_t height = std::max(
            list_bg.get_dimensions().y(),
            Texture(info_bg).get_dimensions().y()
        );
        dimension = { total_width, height };
        dragarea = { static_cast<int16_t>(total_width - 40), 20 };
    }

    void UIQuestLog::initialize_tabs()
    {
        const nl::node tabd0 = questlog_nodes::tab_disabled(0);
        const nl::node tabe0 = questlog_nodes::tab_enabled(0);
        const nl::node tabd1 = questlog_nodes::tab_disabled(1);
        const nl::node tabe1 = questlog_nodes::tab_enabled(1);
        tabs_from_assets = tabd0 && tabe0 && tabd1 && tabe1;
        if (!tabs_from_assets)
        {
            return;
        }

        int16_t tab_x = TAB_X;
        for (uint16_t i = BT_TAB0; i <= BT_TAB1; ++i)
        {
            const auto tab_index = static_cast<uint8_t>(i - BT_TAB0);
            buttons[i] = std::make_unique<TwoSpriteButton>(
                questlog_nodes::tab_disabled(tab_index),
                questlog_nodes::tab_enabled(tab_index),
                Point<int16_t>(tab_x, TAB_Y)
            );
            tab_x = static_cast<int16_t>(
                tab_x + Texture(
                    questlog_nodes::tab_enabled(tab_index)
                ).get_dimensions().x() + TAB_GAP
            );
        }
    }

    void UIQuestLog::rebuild_rows(bool keep_selection)
    {
        const int16_t selected_id =
            keep_selection && selected >= 0
            && selected < static_cast<int16_t>(rows.size())
            ? rows[selected].id
            : -1;

        rows = quest_log_model::collect_rows(quests, filter);

        row_labels.clear();
        row_labels.reserve(rows.size());
        for (const auto& row : rows)
        {
            row_labels.emplace_back(
                Text::A11L,
                Text::LEFT,
                Text::WHITE,
                std::to_string(row.id) + " " + row.name,
                180
            );
        }

        selected = -1;
        if (selected_id >= 0)
        {
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (rows[i].id == selected_id)
                {
                    selected = static_cast<int16_t>(i);
                    break;
                }
            }
        }
        if (selected < 0 && !rows.empty())
        {
            selected = std::min<int16_t>(offset, static_cast<int16_t>(rows.size() - 1));
        }

        const int16_t row_count16 = static_cast<int16_t>(rows.size());
        const int16_t max_offset = std::max<int16_t>(0, static_cast<int16_t>(row_count16 - PAGE_ROWS));
        offset = std::min<int16_t>(offset, max_offset);

        slider.setrows(offset, PAGE_ROWS, std::max<int16_t>(row_count16, 1));
        slider.setenabled(row_count16 > PAGE_ROWS);

        refresh_info_texts();
    }

    void UIQuestLog::refresh_info_texts()
    {
        completed_label.change_text(
            "Completed: " + std::to_string(quests.list_completed().size())
        );

        if (selected >= 0 && selected < static_cast<int16_t>(rows.size()))
        {
            const auto& row = rows[selected];
            selected_name.change_text(row.name);
            selected_category.change_text(
                row.category.empty() ? "Quest" : row.category
            );
            selected_state.change_text(
                filter == Filter::IN_PROGRESS ? "In Progress" : "Completed"
            );
        }
        else
        {
            selected_name.change_text("No quest selected");
            selected_category.change_text("");
            selected_state.change_text("");
        }
    }

    void UIQuestLog::change_filter(Filter new_filter)
    {
        if (new_filter == filter)
        {
            return;
        }

        if (tabs_from_assets)
        {
            buttons[BT_TAB0 + static_cast<uint16_t>(filter)]->set_state(Button::NORMAL);
            buttons[BT_TAB0 + static_cast<uint16_t>(new_filter)]->set_state(Button::PRESSED);
        }
        else
        {
            tab0_label.change_color(
                new_filter == Filter::IN_PROGRESS ? Text::WHITE : Text::LIGHTGREY
            );
            tab1_label.change_color(
                new_filter == Filter::COMPLETED ? Text::WHITE : Text::LIGHTGREY
            );
        }

        filter = new_filter;
        offset = 0;
        rebuild_rows(false);
    }

    void UIQuestLog::change_offset(int16_t new_offset)
    {
        const int16_t row_count16 = static_cast<int16_t>(rows.size());
        const int16_t max_offset =
            std::max<int16_t>(0, static_cast<int16_t>(row_count16 - PAGE_ROWS));
        if (new_offset < 0 || new_offset > max_offset)
        {
            return;
        }

        offset = new_offset;
        slider.setrows(offset, PAGE_ROWS, std::max<int16_t>(row_count16, 1));
    }

    void UIQuestLog::select_row(int16_t row_index)
    {
        if (row_index < 0 || row_index >= static_cast<int16_t>(rows.size()))
        {
            return;
        }

        selected = row_index;
        refresh_info_texts();
    }

    void UIQuestLog::draw(float alpha) const
    {
        draw_sprites(alpha);

        const int16_t row_count16 = static_cast<int16_t>(rows.size());
        Point<int16_t> row_position = position + LIST_OFFSET;
        for (int16_t i = 0; i < PAGE_ROWS; ++i)
        {
            const int16_t index = static_cast<int16_t>(offset + i);
            if (index >= row_count16)
            {
                break;
            }

            if (index == selected)
            {
                selection_box.draw(row_position - Point<int16_t>(4, 4));
            }
            row_labels[index].draw(row_position);
            row_position.shift_y(quest_log_model::ROW_HEIGHT);
        }

        if (!tabs_from_assets)
        {
            tab0_label.draw(position + Point<int16_t>(TAB_X, TAB_Y + 12));
            tab1_label.draw(
                position + Point<int16_t>(TAB_X + 120, TAB_Y + 12)
            );
        }

        completed_label.draw(
            position + Point<int16_t>(LIST_OFFSET.x(), COMPLETED_LABEL_Y)
        );

        const Point<int16_t> info_position =
            position + Point<int16_t>(info_x, 0);
        selected_name.draw(info_position + INFO_NAME_OFFSET);
        selected_category.draw(info_position + INFO_CATEGORY_OFFSET);
        selected_state.draw(info_position + INFO_STATE_OFFSET);

        draw_buttons(alpha);
        slider.draw(position);
    }

    void UIQuestLog::update()
    {
        if (quests.revision() != last_rev)
        {
            last_rev = quests.revision();
            rebuild_rows(true);
        }
    }

    bool UIQuestLog::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (slider.isenabled())
        {
            slider.remove_cursor(clicked);
        }
        return UIDragElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIQuestLog::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            return UIDragElement::send_cursor(clicked, cursorpos);
        }

        const Point<int16_t> cursor_relative = cursorpos - position;
        if (slider.isenabled())
        {
            if (Cursor::State new_state = slider.send_cursor(cursor_relative, clicked))
            {
                return { new_state, true };
            }
        }

        if (cursor_relative.x() >= LIST_OFFSET.x()
            && cursor_relative.x() < static_cast<int16_t>(list_width - SLIDER_MARGIN)
            && cursor_relative.y() >= LIST_OFFSET.y())
        {
            const int16_t row = static_cast<int16_t>(
                (cursor_relative.y() - LIST_OFFSET.y())
                / quest_log_model::ROW_HEIGHT
            );
            const int16_t index = static_cast<int16_t>(offset + row);
            if (row >= 0 && row < PAGE_ROWS
                && index < static_cast<int16_t>(rows.size()))
            {
                if (clicked)
                {
                    select_row(index);
                    return { Cursor::CLICKING, true };
                }
                return { Cursor::CANCLICK, true };
            }
        }

        return UIDragElement::send_cursor(clicked, cursorpos);
    }

    void UIQuestLog::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            deactivate();
        }
    }

    Button::State UIQuestLog::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_TAB0:
            change_filter(Filter::IN_PROGRESS);
            return Button::PRESSED;
        case BT_TAB1:
            change_filter(Filter::COMPLETED);
            return Button::PRESSED;
        default:
            return Button::PRESSED;
        }
    }

    int UIQuestLog::row_count() const
    {
        return static_cast<int>(rows.size());
    }
}
