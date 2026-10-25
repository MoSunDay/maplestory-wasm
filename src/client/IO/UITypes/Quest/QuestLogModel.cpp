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

        std::vector<QuestRow> rows;
        rows.reserve(ids.size());
        for (int16_t id : ids)
        {
            rows.push_back(make_row(id));
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
}
