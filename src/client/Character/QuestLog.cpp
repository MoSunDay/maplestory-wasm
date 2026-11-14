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

#include <chrono>

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
        started_order[qid] = ++started_seq;
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
        // Pair with the most recently STARTED quest carrying that info
        // number, following the insertion-order sequence: `started` is
        // key-sorted, so a later-started smaller-id quest must not hide
        // an earlier parent.
        uint64_t parent_seq = 0;
        for (const auto& entry : started_order)
        {
            if (entry.second > parent_seq
                && quest_info_number(entry.first) == qid)
            {
                parent_seq = entry.second;
                parent = entry.first;
            }
        }
        // Fallback heuristic for servers that repeat the same quest id.
        if (parent == 0 && is_started(qid))
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
            started_order.erase(parent);
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
        started_order.erase(qid);
        in_progress.erase(qid);
        // The quest is gone, so its timer must not keep ticking.
        remove_timer(qid);
        // Remember the info number so update_data can drop the server's
        // trailing info echo instead of resurrecting it as a row. Every
        // forfeit gets its own slot, so a second forfeit cannot let an
        // older echo through; the setfield parser clears the set.
        const int16_t info = quest_info_number(qid);
        if (info != 0)
        {
            forfeited_info_numbers.insert(info);
        }
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
        // can only be the orphan echo: drop it and release its slot.
        if (forfeited_info_numbers.erase(qid) > 0)
        {
            return;
        }

        started[qid] = qdata;
        ++rev;
    }

    void Questlog::complete(int16_t qid, int64_t time)
    {
        started.erase(qid);
        started_order.erase(qid);
        in_progress.erase(qid);
        // Completed quests no longer run their time limit.
        remove_timer(qid);
        completed[qid] = time;
        ++rev;
    }

    void Questlog::clear_forfeited()
    {
        // Stale guards must never suppress rows of a freshly parsed
        // questlog, so the setfield parser empties this at every parse.
        if (forfeited_info_numbers.empty())
        {
            return;
        }

        forfeited_info_numbers.clear();
        ++rev;
    }

    void Questlog::add_timer(int16_t qid, int32_t remaining_ms)
    {
        // Absolute deadline on the steady clock: immune to wall-clock
        // adjustments and directly comparable in timer_remaining_ms.
        const auto now = std::chrono::steady_clock::now()
            .time_since_epoch();
        deadlines[qid] =
            std::chrono::duration_cast<std::chrono::milliseconds>(now)
                .count()
            + remaining_ms;
        ++rev;
    }

    void Questlog::remove_timer(int16_t qid)
    {
        if (deadlines.erase(qid) > 0)
        {
            ++rev;
        }
    }

    int64_t Questlog::timer_remaining_ms(int16_t qid) const
    {
        const auto entry = deadlines.find(qid);
        if (entry == deadlines.end())
        {
            return -1;
        }
        const auto now = std::chrono::steady_clock::now()
            .time_since_epoch();
        const int64_t remaining =
            entry->second
            - std::chrono::duration_cast<std::chrono::milliseconds>(now)
                  .count();
        return remaining > 0 ? remaining : -1;
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
