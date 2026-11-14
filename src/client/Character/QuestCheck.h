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
#include "QuestLog.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace jrc
{
    // A quest interaction available at an NPC: accepting or delivering.
    struct QuestOffer
    {
        int16_t quest_id;
        bool completing; // true = delivery flow, false = accept flow
        bool scripted;   // true = scripted quest (action 4/5), false = standard (1/2)
    };

    // Player context needed by the checks (kept free of Inventory and
    // CharStats dependencies so the module stays independently testable).
    struct QuestCheckContext
    {
        uint16_t level = 0;
        // Player job id (CharStats::get_job().get_id()); the job gate
        // compares it exactly against the Check.img/<qid>/0/job list.
        uint16_t job = 0;
        // Meso balance for the completion-side meso requirement
        // (Check.img/<qid>/1/meso).
        int64_t meso = 0;
        // Wall-clock epoch milliseconds (std::chrono::system_clock at the
        // call site): server completion timestamps are wall-clock too, so
        // the repeat interval compares like against like.
        int64_t now_ms = 0;
        // Returns the total count of an item in the inventory; tests may
        // inject a stub.
        std::function<int32_t(int32_t item_id)> item_count;
    };

    // Finds the quest interaction offered at an NPC: delivery first
    // (progress/item conditions met), then acceptance (smallest quest id).
    // Returns true and fills out when an interaction is available, false
    // otherwise.
    bool quest_offer_at(int32_t npc_id, const Questlog& quests,
        const QuestCheckContext& context, QuestOffer& out);

    int32_t start_npc_of(int16_t qid);   // Check.img/<qid>/0/npc, 0 if absent
    int32_t end_npc_of(int16_t qid);     // Check.img/<qid>/1/npc, 0 if absent
    bool start_scripted(int16_t qid);    // Check.img/<qid>/0/startscript exists
    bool end_scripted(int16_t qid);      // Check.img/<qid>/1/endscript exists
    bool auto_start(int16_t qid);        // QuestInfo.img/<qid>/autoStart == 1
    bool repeatable(int16_t qid);        // Check.img/<qid>/0/interval exists
    std::string quest_name(int16_t qid); // QuestInfo.img/<qid>/name, "?" if absent

    // Job ids accepted for starting a quest: the integer children of
    // Check.img/<qid>/0/job, mirroring the server JobRequirement's
    // processData (the 5-byte encoding only feeds item actions there).
    std::vector<uint16_t> decoded_job_list(int16_t qid);

    // One kill requirement of a running quest, decoded from its progress
    // string.
    struct QuestMobProgress
    {
        int32_t mobid = 0;
        int32_t killed = 0;   // counter decoded from the progress string
        int32_t required = 0; // Check.img/<qid>/1/mob count of that mob
    };

    // Decodes the per-mob kill counters of a running quest's progress
    // string ("000010000" = three digit zero padded counters). Format
    // verified against the linked server: MapleQuestStatus.registerMobs
    // seeds one "000" counter per relevant mob (in
    // MapleQuest.getRelevantMobs order) inside a LinkedHashMap and
    // getQuestData() concatenates the values, so mob #n lives at
    // progress[3n, 3n + 3). One row per Check.img/<qid>/1/mob entry, the
    // killed counter matched by mob id. Returns an empty vector when the
    // string does not cover exactly the tracked mobs (or the quest tracks
    // none); callers then keep the raw string.
    std::vector<QuestMobProgress> decoded_progress(int16_t qid,
        const std::string& progress);
}
