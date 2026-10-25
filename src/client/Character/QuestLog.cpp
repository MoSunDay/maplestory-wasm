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

namespace jrc
{
    void Questlog::add_started(int16_t qid, const std::string& qdata)
    {
        started[qid] = qdata;
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

    bool Questlog::is_started(int16_t qid)
    {
        return started.count(qid) > 0;
    }

    int16_t Questlog::get_last_started()
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
        ++rev;
    }

    void Questlog::update_data(int16_t qid, const std::string& qdata)
    {
        auto iter = started.find(qid);
        if (iter != started.end())
        {
            iter->second = qdata;
        }
        else
        {
            started.emplace(qid, qdata);
        }
        ++rev;
    }

    void Questlog::complete(int16_t qid, int64_t time)
    {
        started.erase(qid);
        in_progress.erase(qid);
        completed[qid] = time;
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
