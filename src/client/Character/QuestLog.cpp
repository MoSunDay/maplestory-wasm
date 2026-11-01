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
#include "QuestLog.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

namespace jrc
{
    int16_t quest_info_number(int16_t qid)
    {
        const nl::node check =
            nl::nx::quest["Check.img"][std::to_string(qid)];
        if (!static_cast<bool>(check))
        {
            return 0;
        }

        // The info number lives in the start ("0") or complete ("1")
        // requirement table; QuestInfo.img carries none.
        for (const char* stage : { "0", "1" })
        {
            const nl::node info = check[stage]["infoNumber"];
            if (static_cast<bool>(info)
                && info.data_type() == nl::node::type::integer)
            {
                return static_cast<int16_t>(info.get_integer());
            }
        }
        return 0;
    }

    void Questlog::add_started(int16_t qid, const std::string& qdata)
    {
        started[qid] = qdata;
        last_started = qid;
        ++rev;
    }

    void Questlog::add_in_progress(int16_t qid, int16_t qidl, const std::string& qdata)
    {
        in_progress[qid] = make_pair(qidl, qdata);
        ++rev;
    }

    void Questlog::add_completed(int16_t qid, int64_t time)
    {
        completed[qid] = time;
        ++rev;
    }

    void Questlog::add_questlog_entry(int16_t qid, const std::string& qdata)
    {
        int16_t parent = 0;

        // Preferred rule: the linked server appends the info entry
        // (keyed by the parent's infoNumber) directly after its parent.
        if (last_started > 0 && quest_info_number(last_started) == qid)
        {
            parent = last_started;
        }
        // Fallback heuristic for servers that repeat the same quest id.
        else if (is_started(qid))
        {
            const int16_t previous = get_last_started();
            if (previous > 0 && previous != qid)
            {
                parent = previous;
            }
        }

        if (parent > 0)
        {
            in_progress[parent] = std::make_pair(qid, qdata);
            started.erase(parent);
            if (last_started == parent)
            {
                last_started = 0;
            }
            ++rev;
        }
        else
        {
            add_started(qid, qdata);
        }
    }

    bool Questlog::is_started(int16_t qid) const
    {
        return started.count(qid) > 0;
    }

    int16_t Questlog::get_last_started() const
    {
        if (started.empty())
        {
            return 0;
        }

        auto qend = started.end();
        qend--;
        return qend->first;
    }

    void Questlog::forfeit(int16_t qid)
    {
        started.erase(qid);
        in_progress.erase(qid);
        if (last_started == qid)
        {
            last_started = 0;
        }
        // Remember the info number so update_data can drop the server's
        // trailing info echo instead of resurrecting it as a row.
        forfeited_info_number = quest_info_number(qid);
        ++rev;
    }

    void Questlog::update_data(int16_t qid, const std::string& qdata)
    {
        // Direct buckets first: the plain quest id updates whichever
        // active bucket holds it.
        auto iter = started.find(qid);
        if (iter != started.end())
        {
            iter->second = qdata;
            ++rev;
            return;
        }

        auto progress = in_progress.find(qid);
        if (progress != in_progress.end())
        {
            progress->second.second = qdata;
            ++rev;
            return;
        }

        // Progress updates for quests with an info sub-quest arrive keyed
        // by the info number; fold them back onto the parent bucket.
        for (const auto& entry : in_progress)
        {
            if (entry.second.first == qid)
            {
                in_progress[entry.first].second = qdata;
                ++rev;
                return;
            }
        }
        for (const auto& entry : started)
        {
            if (quest_info_number(entry.first) == qid)
            {
                in_progress[entry.first] = std::make_pair(qid, qdata);
                started.erase(entry.first);
                ++rev;
                return;
            }
        }

        // Trailing info echo of a forfeit/expire removal. The legitimate
        // re-accept path is already handled by the loop above, so this
        // can only be the orphan echo: drop it.
        if (forfeited_info_number != 0 && qid == forfeited_info_number)
        {
            forfeited_info_number = 0;
            return;
        }

        started[qid] = qdata;
        ++rev;
    }

    void Questlog::complete(int16_t qid, int64_t time)
    {
        started.erase(qid);
        in_progress.erase(qid);
        completed[qid] = time;
        if (last_started == qid)
        {
            last_started = 0;
        }
        ++rev;
    }

    uint64_t Questlog::revision() const
    {
        return rev;
    }

    const std::map<int16_t, std::string>& Questlog::list_started() const
    {
        return started;
    }

    const std::map<int16_t, std::pair<int16_t, std::string>>&
        Questlog::list_in_progress() const
    {
        return in_progress;
    }

    const std::map<int16_t, int64_t>& Questlog::list_completed() const
    {
        return completed;
    }
}
