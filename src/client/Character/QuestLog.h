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
#include <map>
#include <set>
#include <string>

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
        // info number of the most recently STARTED quest pairs up with it
        // as a single in-progress record (the linked server appends the
        // info entry right after its parent; "most recently" follows the
        // insertion-order sequence, not the key-sorted map order). A
        // repeated quest id keeps the legacy same-id pairing heuristic
        // as the fallback.
        void add_questlog_entry(int16_t qid, const std::string& quest_data);

        bool is_started(int16_t) const;
        int16_t get_last_started() const;

        // Remove a quest from both active lists (forfeit or done).
        void forfeit(int16_t qid);
        // Refresh a started quest's progress data, adding it if unknown.
        void update_data(int16_t qid, const std::string& quest_data);
        // Move a quest into the completed list with a completion time.
        void complete(int16_t qid, int64_t time);
        // Drops every remembered forfeited info number. The setfield
        // parser calls this at the start of each questlog parse so stale
        // guards from a previous field never suppress fresh rows.
        void clear_forfeited();

        // Server quest timers (UPDATE_QUEST_INFO modes 6/7): add_timer
        // converts the remaining milliseconds into an absolute deadline
        // on the steady clock; remove_timer also runs from forfeit and
        // complete so timers die with the quest.
        void add_timer(int16_t qid, int32_t remaining_ms);
        void remove_timer(int16_t qid);
        // Milliseconds left on the quest's timer; -1 when the quest has
        // no timer or the deadline already passed.
        int64_t timer_remaining_ms(int16_t qid) const;

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
        // Insertion-order bookkeeping for the started quests: `started`
        // is key-sorted, so this sequence counter is the only record of
        // which quest started most recently. add_questlog_entry pairs an
        // info entry with the highest-sequence started quest carrying
        // the matching infoNumber.
        std::map<int16_t, uint64_t> started_order;
        uint64_t started_seq = 0;
        // Info numbers of forfeited quests. The linked server trails a
        // forfeit/expire removal with one info update keyed by exactly
        // that id (NOT_STARTED branch of updateQuest); by the time it
        // arrives every bucket that could own it is gone, so the
        // update_data fallback must not resurrect it as a row. One slot
        // per forfeit (not a single value) so a second forfeit cannot
        // let an older forfeited info entry resurrect.
        std::set<int16_t> forfeited_info_numbers;
        // Quest id -> absolute deadline in steady-clock milliseconds
        // (server addQuestTimeLimit/removeQuestTimeLimit).
        std::map<int16_t, int64_t> deadlines;
    };
}

