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
#include "QuestCheck.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace jrc
{
    namespace
    {
        // Check.img/<qid>/<stage> ("0" = start table, "1" = complete table).
        nl::node check_stage(int16_t qid, const char* stage)
        {
            return nl::nx::quest["Check.img"][std::to_string(qid)][stage];
        }

        nl::node quest_info(int16_t qid)
        {
            return nl::nx::quest["QuestInfo.img"][std::to_string(qid)];
        }

        bool node_is_integer(const nl::node& node)
        {
            return static_cast<bool>(node)
                && node.data_type() == nl::node::type::integer;
        }

        int32_t read_int(const nl::node& node)
        {
            return node_is_integer(node)
                ? static_cast<int32_t>(node.get_integer())
                : 0;
        }

        int16_t quest_id_of(const nl::node& quest)
        {
            const int qid = std::atoi(quest.name().c_str());
            return qid > 0 && qid <= 0x7FFF ? static_cast<int16_t>(qid) : 0;
        }

        // Reverse index over Check.img, built once on first use.
        struct NpcIndex
        {
            std::map<int32_t, std::vector<int16_t>> start;
            std::map<int32_t, std::vector<int16_t>> end;
        };

        const NpcIndex& npc_index()
        {
            static const NpcIndex index = [] {
                NpcIndex built;
                const nl::node root = nl::nx::quest["Check.img"];
                for (const nl::node& quest : root)
                {
                    const int16_t qid = quest_id_of(quest);
                    if (qid == 0)
                        continue;
                    const int32_t startnpc = read_int(quest["0"]["npc"]);
                    const int32_t endnpc = read_int(quest["1"]["npc"]);
                    if (startnpc > 0)
                        built.start[startnpc].push_back(qid);
                    if (endnpc > 0)
                        built.end[endnpc].push_back(qid);
                }
                return built;
            }();
            return index;
        }

        // Kill-count order shared with the server's progress encoding:
        // MapleQuestStatus.registerMobs seeds one "000" counter per relevant
        // mob in MapleQuest.getRelevantMobs() order - Check.img/<qid>/0/mob
        // ids first, then Check.img/<qid>/1/mob ids, deduplicated while
        // preserving the first occurrence - and getQuestData() concatenates
        // the counters. Mob #n therefore lives at progress[3n, 3n + 3) as a
        // three digit zero padded decimal.
        const std::vector<int32_t>& relevant_mobs(int16_t qid)
        {
            static const std::map<int16_t, std::vector<int32_t>> table = [] {
                std::map<int16_t, std::vector<int32_t>> built;
                const nl::node root = nl::nx::quest["Check.img"];
                for (const nl::node& quest : root)
                {
                    const int16_t qid = quest_id_of(quest);
                    if (qid == 0)
                        continue;
                    std::vector<int32_t> mobs;
                    for (const char* stage : { "0", "1" })
                    {
                        const nl::node list = quest[stage]["mob"];
                        for (const nl::node& entry : list)
                        {
                            const int32_t id = read_int(entry["id"]);
                            if (id > 0
                                && std::find(mobs.begin(), mobs.end(), id)
                                    == mobs.end())
                            {
                                mobs.push_back(id);
                            }
                        }
                    }
                    if (!mobs.empty())
                        built.emplace(qid, std::move(mobs));
                }
                return built;
            }();
            static const std::vector<int32_t> none;
            const auto entry = table.find(qid);
            return entry != table.end() ? entry->second : none;
        }

        // Decodes one mob's kill counter out of the quest's progress string
        // (order = relevant mob table of that quest); a mob missing from the
        // table or a short string simply counts as zero.
        int32_t progress_counter(const std::string& progress, int32_t mob_id,
            const std::vector<int32_t>& order)
        {
            const auto slot = std::find(order.begin(), order.end(), mob_id);
            if (slot == order.end())
                return 0;
            const size_t offset =
                static_cast<size_t>(slot - order.begin()) * 3;
            if (progress.size() < offset + 3)
                return 0;
            return std::atoi(progress.substr(offset, 3).c_str());
        }

        // Progress data of an active quest; the started table wins over the
        // in-progress pairing.
        std::string progress_of(const Questlog& quests, int16_t qid)
        {
            const auto started = quests.list_started().find(qid);
            if (started != quests.list_started().end())
                return started->second;
            const auto paired = quests.list_in_progress().find(qid);
            if (paired != quests.list_in_progress().end())
                return paired->second.second;
            return std::string();
        }

        // Meso gate (server MesoRequirement: chr.getMeso() >= meso).
        // Used on the completion table; a start-side shortfall is
        // reported by the server via UPDATE_QUEST_INFO 0x0B instead.
        bool meso_met(int16_t qid, const QuestCheckContext& context)
        {
            const int64_t required =
                read_int(check_stage(qid, "1")["meso"]);
            return context.meso >= required;
        }

        // Completion conditions live in Check.img/<qid>/1: every mob entry
        // needs its decoded kill counter >= count and every item entry needs
        // context.item_count(id) >= count. A table without mob and item
        // nodes is unconditionally deliverable, mirroring the server whose
        // canComplete() only checks the npc then.
        bool completion_met(int16_t qid, const Questlog& quests,
            const QuestCheckContext& context)
        {
            const std::string progress = progress_of(quests, qid);
            const std::vector<int32_t>& mobs = relevant_mobs(qid);
            const nl::node table = check_stage(qid, "1");
            for (const nl::node& entry : table["mob"])
            {
                const int32_t count = read_int(entry["count"]);
                const int32_t killed = progress_counter(
                    progress, read_int(entry["id"]), mobs);
                if (killed < count)
                    return false;
            }
            for (const nl::node& entry : table["item"])
            {
                const int32_t id = read_int(entry["id"]);
                const int32_t count = read_int(entry["count"]);
                if (!context.item_count || context.item_count(id) < count)
                    return false;
            }
            // Completion-side meso (server MesoRequirement over
            // Check.img/<qid>/1/meso) is checked like the item counts.
            if (!meso_met(qid, context))
                return false;
            return true;
        }

        // Quest prerequisites from Check.img/<qid>/0/quest/<i>: the
        // referenced quest must sit in the exact required state (server
        // QuestRequirement, MapleQuestStatus.Status: 0 NOT_STARTED,
        // 1 STARTED, 2 COMPLETED; the state node defaults to 0 when
        // absent). "Active" counts both local buckets, started and the
        // in-progress pairing.
        bool prerequisites_met(int16_t qid, const Questlog& quests)
        {
            const nl::node list = check_stage(qid, "0")["quest"];
            for (const nl::node& entry : list)
            {
                const int32_t ref = read_int(entry["id"]);
                const int32_t state = read_int(entry["state"]);
                if (ref <= 0 || ref > 0x7FFF)
                    continue;
                const int16_t refid = static_cast<int16_t>(ref);
                if (state == 2)
                {
                    if (quests.list_completed().count(refid) == 0)
                        return false;
                }
                else if (state == 1)
                {
                    if (!quests.is_started(refid)
                        && quests.list_in_progress().count(refid) == 0)
                        return false;
                }
                else if (state == 0)
                {
                    // NOT_STARTED: the quest must sit in no bucket at
                    // all (an unknown quest counts as NOT_STARTED on the
                    // server, hence the pass-through above).
                    if (quests.is_started(refid)
                        || quests.list_in_progress().count(refid) > 0
                        || quests.list_completed().count(refid) > 0)
                        return false;
                }
            }
            return true;
        }

        // Minimum level gate. The shipped Check.img names the node "lvmin"
        // (1973 occurrences; a plain "lv" node does not exist).
        bool level_met(int16_t qid, const QuestCheckContext& context)
        {
            const nl::node lvmin = check_stage(qid, "0")["lvmin"];
            if (!node_is_integer(lvmin))
                return true;
            const int64_t required = lvmin.get_integer();
            return required <= 0
                || context.level >= static_cast<uint16_t>(required);
        }

        // Maximum level gate (server MaxLevelRequirement:
        // maxLevel >= level; absent or <= 0 never restricts).
        bool lvmax_met(int16_t qid, const QuestCheckContext& context)
        {
            const nl::node lvmax = check_stage(qid, "0")["lvmax"];
            if (!node_is_integer(lvmax))
                return true;
            const int64_t limit = lvmax.get_integer();
            return limit <= 0
                || context.level <= static_cast<uint16_t>(limit);
        }

        // Job gate (server JobRequirement.check): the player job must be
        // one of the listed ids. The server also lets GM accounts through
        // (chr.isGM()); the client cannot observe that account flag, so
        // a table listing the GM-side id 2000 keeps the local fallback
        // offer alive and lets the server reject the actual request.
        bool job_met(int16_t qid, const QuestCheckContext& context)
        {
            const std::vector<uint16_t> jobs = decoded_job_list(qid);
            if (jobs.empty())
                return true;
            for (const uint16_t job : jobs)
            {
                if (job == context.job || job == 2000)
                    return true;
            }
            return false;
        }

        // Stored completion times carry whatever unit their wire path
        // used: setfield sends Windows FILETIME (100 ns since 1601,
        // magnitudes above 1e17), the local completion path stores plain
        // epoch seconds. Reduce them all to wall-clock epoch ms.
        int64_t completion_ms(int64_t stored)
        {
            if (stored > 116444736000000000LL)
                return (stored - 116444736000000000LL) / 10000LL;
            if (stored > 100000000000LL)
                return stored;
            return stored > 0 ? stored * 1000 : 0;
        }

        // Repeat cooldown (server IntervalRequirement.processData: the
        // node value is minutes): a completed quest re-becomes acceptable
        // once now - completion >= interval minutes. Only repeatable
        // quests reach this check (acceptable drops the rest earlier).
        // A caller that supplies no wall clock (now_ms <= 0) cannot be
        // compared, so the gate stays open and the server re-checks.
        bool interval_elapsed(
            int16_t qid, const Questlog& quests, int64_t now_ms)
        {
            if (now_ms <= 0)
                return true;
            const nl::node interval = check_stage(qid, "0")["interval"];
            if (!node_is_integer(interval))
                return true;
            const int64_t cooldown_ms =
                interval.get_integer() * 60LL * 1000LL;
            const auto done = quests.list_completed().find(qid);
            if (done == quests.list_completed().end())
                return true;
            return now_ms - completion_ms(done->second) >= cooldown_ms;
        }

        bool deliverable(int16_t qid, const Questlog& quests,
            const QuestCheckContext& context)
        {
            const bool active = quests.is_started(qid)
                || quests.list_in_progress().count(qid) > 0;
            if (!active || quests.list_completed().count(qid) > 0)
                return false;
            return completion_met(qid, quests, context);
        }

        bool acceptable(int16_t qid, const Questlog& quests,
            const QuestCheckContext& context)
        {
            if (quests.is_started(qid)
                || quests.list_in_progress().count(qid) > 0)
                return false;
            const bool done = quests.list_completed().count(qid) > 0;
            if (done && !repeatable(qid))
                return false;
            if (auto_start(qid))
                return false;
            if (!level_met(qid, context))
                return false;
            if (!lvmax_met(qid, context))
                return false;
            if (!job_met(qid, context))
                return false;
            if (done && !interval_elapsed(qid, quests, context.now_ms))
                return false;
            return prerequisites_met(qid, quests);
        }

        // Smallest acceptable quest started by the npc, or 0.
        int16_t smallest_offer(const std::vector<int16_t>& quests,
            bool (*eligible)(int16_t, const Questlog&,
                const QuestCheckContext&),
            const Questlog& log, const QuestCheckContext& context)
        {
            int16_t best = 0;
            for (int16_t qid : quests)
            {
                if ((best == 0 || qid < best)
                    && eligible(qid, log, context))
                {
                    best = qid;
                }
            }
            return best;
        }
    }

    bool quest_offer_at(int32_t npc_id, const Questlog& quests,
        const QuestCheckContext& context, QuestOffer& out)
    {
        const NpcIndex& index = npc_index();
        const auto endlist = index.end.find(npc_id);
        if (endlist != index.end.end())
        {
            const int16_t qid =
                smallest_offer(endlist->second, deliverable, quests, context);
            if (qid != 0)
            {
                out = QuestOffer{ qid, true, end_scripted(qid) };
                return true;
            }
        }
        const auto startlist = index.start.find(npc_id);
        if (startlist != index.start.end())
        {
            const int16_t qid =
                smallest_offer(startlist->second, acceptable, quests, context);
            if (qid != 0)
            {
                out = QuestOffer{ qid, false, start_scripted(qid) };
                return true;
            }
        }
        return false;
    }

    int32_t start_npc_of(int16_t qid)
    {
        return read_int(check_stage(qid, "0")["npc"]);
    }

    int32_t end_npc_of(int16_t qid)
    {
        return read_int(check_stage(qid, "1")["npc"]);
    }

    bool start_scripted(int16_t qid)
    {
        return static_cast<bool>(check_stage(qid, "0")["startscript"]);
    }

    bool end_scripted(int16_t qid)
    {
        return static_cast<bool>(check_stage(qid, "1")["endscript"]);
    }

    bool auto_start(int16_t qid)
    {
        const nl::node flag = quest_info(qid)["autoStart"];
        return node_is_integer(flag) && flag.get_integer() == 1;
    }

    bool repeatable(int16_t qid)
    {
        return static_cast<bool>(check_stage(qid, "0")["interval"]);
    }

    std::vector<uint16_t> decoded_job_list(int16_t qid)
    {
        // Server JobRequirement.processData collects the integer children
        // of the job node; the 5-byte encoding only feeds item actions
        // (ItemAction.canGetItem) and never the quest start gate.
        std::vector<uint16_t> jobs;
        for (const nl::node& entry : check_stage(qid, "0")["job"])
        {
            if (node_is_integer(entry))
            {
                jobs.push_back(
                    static_cast<uint16_t>(entry.get_integer()));
            }
        }
        return jobs;
    }

    std::string quest_name(int16_t qid)
    {
        const nl::node name = quest_info(qid)["name"];
        return static_cast<bool>(name) ? name.get_string() : "?";
    }

    std::vector<QuestMobProgress> decoded_progress(int16_t qid,
        const std::string& progress)
    {
        std::vector<QuestMobProgress> kills;
        const std::vector<int32_t>& mobs = relevant_mobs(qid);
        // Length gate: the server always writes one padded counter per
        // tracked mob, so any other length is a foreign format the
        // caller must keep verbatim.
        if (mobs.empty() || progress.size() != mobs.size() * 3)
        {
            return kills;
        }

        // Rows follow the completion table; the killed counter is
        // matched by mob id (the server also keys progress by mob id),
        // which keeps the pairing correct when the start table tracks
        // extra mobs the completion side never asks for.
        for (const nl::node& entry : check_stage(qid, "1")["mob"])
        {
            QuestMobProgress kill;
            kill.mobid = read_int(entry["id"]);
            kill.required = read_int(entry["count"]);
            kill.killed = progress_counter(progress, kill.mobid, mobs);
            kills.push_back(kill);
        }
        return kills;
    }
}
