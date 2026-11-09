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
#include "QuestDialogModel.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>

namespace
{
    // Say.img/<qid>/<stage>: "0" is the accept speech, "1" the delivery
    // speech. Say.img/<qid>/<stage>/no holds the decline replies.
    nl::node say_stage(int16_t qid, const char* stage)
    {
        return nl::nx::quest["Say.img"][std::to_string(qid)][stage];
    }

    // QuestInfo.img/<qid> metadata entry.
    nl::node quest_info(int16_t qid)
    {
        return nl::nx::quest["QuestInfo.img"][std::to_string(qid)];
    }

    bool node_is_string(const nl::node& node)
    {
        return static_cast<bool>(node)
            && node.data_type() == nl::node::type::string;
    }

    std::string string_of(const nl::node& node)
    {
        return node_is_string(node) ? node.get_string() : std::string();
    }

    // True when the child name is only digits ("0", "1", "2", ...). The
    // "no" decline subtree and any named metadata node are rejected.
    bool is_numbered_child(const std::string& name)
    {
        return !name.empty()
            && name.find_first_not_of("0123456789") == std::string::npos;
    }

    // The numbered non-empty string children of `parent`, ordered by their
    // numeric node name. The NX tables already store children sorted by
    // name; the map makes that explicit so "10" never sorts before "2".
    std::vector<std::string> numbered_strings(const nl::node& parent)
    {
        std::map<int32_t, std::string> by_index;
        for (const nl::node& child : parent)
        {
            if (!is_numbered_child(child.name()))
            {
                continue;
            }

            const std::string value = string_of(child);
            if (value.empty())
            {
                continue;
            }

            by_index[std::atoi(child.name().c_str())] = value;
        }

        std::vector<std::string> pages;
        pages.reserve(by_index.size());
        for (const auto& entry : by_index)
        {
            pages.push_back(entry.second);
        }
        return pages;
    }

    // Decline reply page Say.img/<qid>/<stage>/no/0; empty when the data
    // files ship none (the dialog then closes right away on NO).
    std::string decline_of(int16_t qid, const char* stage)
    {
        return string_of(say_stage(qid, stage)["no"]["0"]);
    }

    // QuestInfo.img/<qid>/<stage> summary paragraph; only string nodes
    // count, anything else (autoStart flags, integer nodes) is ignored.
    std::string summary_of(int16_t qid, const char* stage)
    {
        return string_of(quest_info(qid)[stage]);
    }

    // Last-resort line so the offer stays answerable without any data.
    const char* fallback_line(bool completing)
    {
        return completing
            ? "确定要提交这个任务吗？"
            : "你愿意接受这个任务吗？";
    }
}

namespace jrc
{
    namespace quest_dialog_model
    {
        DialogContent collect(int16_t qid, bool completing)
        {
            const char* stage = completing ? "1" : "0";

            DialogContent content;
            content.pages = numbered_strings(say_stage(qid, stage));
            content.decline_page = decline_of(qid, stage);

            // Say-less quests borrow the QuestInfo summary paragraph; quests
            // without even that keep the Chinese fallback question.
            if (content.pages.empty())
            {
                const std::string summary = summary_of(qid, stage);
                if (!summary.empty())
                {
                    content.pages.push_back(summary);
                }
                else
                {
                    content.pages.push_back(fallback_line(completing));
                }
            }

            content.title = string_of(quest_info(qid)["name"]);
            if (content.title.empty())
            {
                content.title = "任务";
            }

            return content;
        }
    }
}
