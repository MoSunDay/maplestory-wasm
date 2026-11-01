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
#include <map>

namespace jrc
{
    // Info-number of a quest: Quest.nx Check.img/<qid>/0/infoNumber with
    // a /1 fallback (the start/complete requirement tables). 0 when the
    // quest has no info sub-quest. Shared by the setfield parser and the
    // runtime progress updates; QuestInfo.img carries no infoNumber.
    int16_t quest_info_number(int16_t qid);

    // Class that stores information on the questlog of an individual character.
    // Grandfathered class: the container API is kept, every mutator bumps the
    // revision counter so UI windows can cheaply detect changes.
    class Questlog
    {
    public:
        void add_started(int16_t, const std::string& quest_data);
        void add_in_progress(int16_t, int16_t, const std::string& quest_data);
        void add_completed(int16_t, int64_t);

        // Fold one questlog wire entry in: an entry whose id matches the
        // info number of the previously added started quest pairs up with
        // it as a single in-progress record (the linked server appends
        // the info entry right after its parent). A repeated quest id
        // keeps the legacy same-id pairing heuristic as the fallback.
        void add_questlog_entry(int16_t qid, const std::string& quest_data);

        bool is_started(int16_t) const;
        int16_t get_last_started() const;

        // Remove a quest from both active lists (forfeit or done).
        void forfeit(int16_t qid);
        // Refresh a started quest's progress data, adding it if unknown.
        void update_data(int16_t qid, const std::string& quest_data);
        // Move a quest into the completed list with a completion time.
        void complete(int16_t qid, int64_t time);

        // Monotonic counter bumped by every mutator call.
        uint64_t revision() const;

        const std::map<int16_t, std::string>& list_started() const;
        const std::map<int16_t, std::pair<int16_t, std::string>>&
            list_in_progress() const;
        const std::map<int16_t, int64_t>& list_completed() const;

    private:
        std::map<int16_t, std::string> started;
        std::map<int16_t, std::pair<int16_t, std::string>> in_progress;
        std::map<int16_t, int64_t> completed;
        uint64_t rev = 0;
        // qid of the most recent add_started; only meaningful within one
        // setfield parse, where it pairs an info entry with its parent.
        int16_t last_started = 0;
    };
}

