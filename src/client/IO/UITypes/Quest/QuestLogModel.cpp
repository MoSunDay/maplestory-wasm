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
#include "QuestLogModel.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <set>

namespace
{
    using jrc::quest_log_model::QuestRow;

    // QuestInfo metadata for one quest id. Names stay empty when the quest
    // is unknown to the data files; the callers add the "?" fallback.
    void load_info(int16_t id, QuestRow& row)
    {
        const nl::node info =
            nl::nx::quest["QuestInfo.img"][std::to_string(id)];
        if (!static_cast<bool>(info))
        {
            return;
        }

        const nl::node name = info["name"];
        if (static_cast<bool>(name))
        {
            row.name = name.get_string();
        }

        const nl::node parent = info["parent"];
        if (static_cast<bool>(parent))
        {
            row.category = parent.get_string();
        }
    }

    QuestRow make_row(int16_t id)
    {
        QuestRow row;
        row.id = id;
        load_info(id, row);
        if (row.name.empty())
        {
            row.name = "?" + std::to_string(id);
        }
        return row;
    }
}

namespace jrc::quest_log_model
{
    std::vector<QuestRow> collect_rows(const Questlog& log, Filter filter)
    {
        // Map keys are sorted already; a set also merges the two active
        // lists so a quest tracked as both started and in progress (the
        // setfield parser splits chains across both) shows one row.
        std::set<int16_t> ids;
        if (filter == Filter::COMPLETED)
        {
            for (const auto& entry : log.list_completed())
            {
                ids.insert(entry.first);
            }
        }
        else
        {
            for (const auto& entry : log.list_started())
            {
                ids.insert(entry.first);
            }
            for (const auto& entry : log.list_in_progress())
            {
                ids.insert(entry.first);
            }
        }

        const auto& in_progress = log.list_in_progress();
        const auto& started = log.list_started();
        const auto& completed = log.list_completed();

        std::vector<QuestRow> rows;
        rows.reserve(ids.size());
        for (int16_t id : ids)
        {
            QuestRow row = make_row(id);
            if (filter == Filter::COMPLETED)
            {
                row.completed_time = completed.at(id);
            }
            else
            {
                // In-progress rows carry the raw progress string when the
                // server sent one; a quest that also sits in the completed
                // bucket keeps its completion time for the info pane.
                const auto running = in_progress.find(id);
                if (running != in_progress.end())
                {
                    row.progress = running->second.second;
                }
                else
                {
                    const auto begun = started.find(id);
                    if (begun != started.end())
                    {
                        row.progress = begun->second;
                    }
                }
                if (row.progress.empty())
                {
                    const auto done = completed.find(id);
                    if (done != completed.end())
                    {
                        row.completed_time = done->second;
                    }
                }
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }

    std::string quest_summary(int16_t id)
    {
        QuestRow row;
        load_info(id, row);
        if (row.name.empty())
        {
            return "?" + std::to_string(id);
        }

        if (row.category.empty())
        {
            return row.name;
        }
        return row.category + " - " + row.name;
    }

    std::string format_completed_time(int64_t time)
    {
        if (time <= 0)
        {
            return "?";
        }

        // Setfield sends Windows FILETIME (100ns since 1601), runtime
        // completions store plain epoch seconds; tell them apart by
        // magnitude.
        int64_t seconds = time;
        if (time > 1000000000000000LL)
        {
            seconds = (time - 116444736000000000LL) / 10000000LL;
        }

        const std::time_t epoch = static_cast<std::time_t>(seconds);
        const std::tm* utc = std::gmtime(&epoch);
        if (!utc)
        {
            return "?";
        }

        char buffer[16] = {};
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", utc) == 0)
        {
            return "?";
        }
        return buffer;
    }

    std::vector<std::string> quest_paragraphs(int16_t id)
    {
        const nl::node info =
            nl::nx::quest["QuestInfo.img"][std::to_string(id)];
        if (!static_cast<bool>(info))
        {
            return {};
        }

        // The story text lives in the numbered string children; the cap
        // guards against runaway nodes on hand-edited data.
        std::vector<std::string> paragraphs;
        for (int i = 0; i < 32; ++i)
        {
            const nl::node paragraph = info[std::to_string(i)];
            if (paragraph.data_type() != nl::node::type::string)
            {
                break;
            }
            paragraphs.push_back(paragraph.get_string());
        }
        return paragraphs;
    }
}
