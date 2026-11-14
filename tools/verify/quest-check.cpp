// Standalone verifier for the QuestCheck module: the Check.img reverse
// npc index, the script/autoStart/interval flags, QuestInfo names, the
// lvmin/lvmax/job/quest-state/interval accept gates, and the accept/deliver
// offer resolution (mob progress decoding plus item stubs)
// against the shipped Quest.nx. Data facts cross-checked with
// link_repos/MapleStory-Server/wz/Quest.wz/Check.img.xml and QuestInfo.img.xml.
//
// Links only the pure model plus nlnx, so no graphics state is needed.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function
//       -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes
//       tools/verify/quest-check.cpp
//       src/client/Character/QuestCheck.cpp
//       src/client/Character/QuestLog.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp
//       src/nlnx/nx.cpp src/client/includes/lz4.c
//       -o /tmp/quest-check
//   /tmp/quest-check assets/Quest.nx

#include <cstdint>
#include <iostream>
#include <string>

#include "Character/QuestCheck.h"
#include "Character/QuestLog.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

namespace nl
{
    // Metadata-only bitmap implementation; the real decoder lives in the
    // client and is never exercised by the quest checks.
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}

    namespace nx
    {
        // Defined in nx.cpp; opens the file and returns its root node.
        node add_file(std::string name);
    }
}

namespace
{
    int failures = 0;

    void check(bool ok, const std::string& what)
    {
        if (!ok)
        {
            std::cerr << "FAIL: " << what << "\n";
            ++failures;
        }
    }

    // quest_offer_at reports whether an interaction was found and fills a
    // caller-owned QuestOffer; same_offer matches both parts.
    bool same_offer(bool found, const jrc::QuestOffer& offer,
        int16_t quest_id, bool completing, bool scripted)
    {
        return found
            && offer.quest_id == quest_id
            && offer.completing == completing
            && offer.scripted == scripted;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: quest-check Quest.nx\n";
        return 2;
    }

    nl::nx::quest = nl::nx::add_file(argv[1]);

    using jrc::Questlog;

    // --- reverse index over Check.img (facts from Check.img.xml) ---
    check(jrc::start_npc_of(1000) == 2101, "quest 1000 starts at npc 2101");
    check(jrc::end_npc_of(1000) == 2100, "quest 1000 delivers at npc 2100");
    check(jrc::start_npc_of(32000) == 0, "unknown quest has no start npc");
    check(jrc::end_npc_of(32000) == 0, "unknown quest has no end npc");

    // --- script / autoStart / interval / name flags ---
    check(jrc::start_scripted(21000), "quest 21000 carries a startscript");
    check(!jrc::start_scripted(1000), "quest 1000 has no startscript");
    check(jrc::end_scripted(10002), "quest 10002 carries an endscript");
    check(!jrc::end_scripted(1000), "quest 1000 has no endscript");
    check(jrc::auto_start(10000), "quest 10000 is autoStart");
    check(!jrc::auto_start(1000), "quest 1000 is not autoStart");
    check(jrc::repeatable(10002), "quest 10002 has an interval node");
    check(!jrc::repeatable(1000), "quest 1000 has no interval node");
    check(!jrc::quest_name(1000).empty() && jrc::quest_name(1000) != "?",
        "quest 1000 resolves a QuestInfo name");
    check(jrc::quest_name(32000) == "?", "unknown quest name falls back");

    // --- accept side ---
    jrc::QuestCheckContext novice;
    novice.level = 10;
    novice.item_count = [](int32_t) { return 0; };

    // One caller-owned slot reused across calls: quest_offer_at no longer
    // parks its result in a shared static, so back-to-back calls cannot
    // clobber each other.
    jrc::QuestOffer offer{};
    bool found = false;

    Questlog empty;
    found = jrc::quest_offer_at(2101, empty, novice, offer);
    check(same_offer(found, offer, 1000, false, false),
        "npc 2101 offers quest 1000 to a fresh character");
    // Quest 1001 (the only quest starting at npc 2100) requires quest 1000
    // completed first, so an empty log yields no offer there.
    found = jrc::quest_offer_at(2100, empty, novice, offer);
    check(!found, "npc 2100 offers nothing on an empty log");

    Questlog done1001;
    done1001.add_completed(1001, 1);
    found = jrc::quest_offer_at(2100, done1001, novice, offer);
    check(!found, "completed non-repeatable quest is not re-offered");

    // Quest 10002 (start npc 9000034, lvmin 10, interval node) needs quest
    // 10001 completed; completed + repeatable quests are re-offered.
    Questlog agent;
    agent.add_completed(10001, 1);
    agent.add_completed(10002, 1);
    found = jrc::quest_offer_at(9000034, agent, novice, offer);
    check(same_offer(found, offer, 10002, false, false),
        "completed repeatable quest is re-offered");

    // Quest 21000 (start npc 1209000, startscript q21000s) is a scripted
    // accept even though its Check/0 lists no lvmin.
    found = jrc::quest_offer_at(1209000, empty, novice, offer);
    check(same_offer(found, offer, 21000, false, true),
        "npc 1209000 offers scripted quest 21000");

    // --- deliver side ---
    // Quest 1000's Check/1 carries only the npc node (no mob, no item), so
    // it is unconditionally deliverable once started, matching the server's
    // canComplete() which only checks the npc.
    Questlog started1000;
    started1000.add_started(1000, "");
    found = jrc::quest_offer_at(2100, started1000, novice, offer);
    check(same_offer(found, offer, 1000, true, false),
        "started quest 1000 is deliverable at npc 2100");

    // Quest 1001 gates delivery on Check/1/item: id 4031003 count 1.
    jrc::QuestCheckContext with_mirror;
    with_mirror.level = 10;
    with_mirror.item_count = [](int32_t item_id) {
        return item_id == 4031003 ? 1 : 0;
    };

    Questlog mirror;
    mirror.add_started(1000, "");
    mirror.add_started(1001, "");
    found = jrc::quest_offer_at(2101, mirror, with_mirror, offer);
    check(same_offer(found, offer, 1001, true, false),
        "quest 1001 is deliverable with the required item");
    found = jrc::quest_offer_at(2101, mirror, novice, offer);
    check(!found || !offer.completing,
        "quest 1001 stays undeliverable without the required item");

    // Mob progress decoding (quest 1016, npc 12100: Check/1/mob 100100 x5,
    // 100101 x5, 120100 x3). The progress string concatenates three digit
    // zero padded counters in relevant-mob order, see
    // MapleQuestStatus.registerMobs/getQuestData.
    Questlog hunting;
    hunting.add_started(1016, "005005003");
    found = jrc::quest_offer_at(12100, hunting, novice, offer);
    check(found && offer.completing,
        "quest 1016 counts as complete once the mob counters are met");

    Questlog hunting_short;
    hunting_short.add_started(1016, "004005003");
    found = jrc::quest_offer_at(12100, hunting_short, novice, offer);
    check(!found || !offer.completing,
        "quest 1016 stays incomplete while a mob counter is short");

    // A completed quest is never a delivery candidate.
    Questlog closed;
    closed.add_started(1016, "005005003");
    closed.add_completed(1016, 1);
    found = jrc::quest_offer_at(12100, closed, novice, offer);
    check(!found || !offer.completing,
        "completed quest is not offered for delivery");

    // --- level ceiling: Check/0/lvmax (quest 1201, npc 9020000,
    // lvmin 21 / lvmax 30, no job/prereq gates). ---
    jrc::QuestCheckContext in_band;
    in_band.level = 25;
    in_band.item_count = novice.item_count;
    found = jrc::quest_offer_at(9020000, empty, in_band, offer);
    check(same_offer(found, offer, 1201, false, false),
        "quest 1201 is offered inside its lvmin..lvmax band");

    jrc::QuestCheckContext over_band = in_band;
    over_band.level = 31;
    found = jrc::quest_offer_at(9020000, empty, over_band, offer);
    check(!found, "quest 1201 is hidden above lvmax 30");

    jrc::QuestCheckContext under_band = in_band;
    under_band.level = 20;
    found = jrc::quest_offer_at(9020000, empty, under_band, offer);
    check(!found, "quest 1201 is hidden below lvmin 21");

    // --- job gate: Check/0/job (quest 20010, npc 1102004, jobs [1000],
    // lvmax 10, startscript -> scripted offer). ---
    jrc::QuestCheckContext warrior;
    warrior.level = 10;
    warrior.job = 1000;
    warrior.item_count = novice.item_count;
    found = jrc::quest_offer_at(1102004, empty, warrior, offer);
    check(same_offer(found, offer, 20010, false, true),
        "quest 20010 is offered to a level-10 job-1000 character");

    jrc::QuestCheckContext mage = warrior;
    mage.job = 200;
    found = jrc::quest_offer_at(1102004, empty, mage, offer);
    check(!found, "quest 20010 is hidden from another job family");

    jrc::QuestCheckContext over_job_cap = warrior;
    over_job_cap.level = 11;
    found = jrc::quest_offer_at(1102004, empty, over_job_cap, offer);
    check(!found, "quest 20010 lvmax 10 also gates the upper bound");

    // --- quest-state prerequisite: Check/0/quest id+state
    // (quest 4014, npc 9330028, requires 4013 in state 1 = STARTED;
    // quest 1001 requires 1000 in state 2 = COMPLETED). ---
    found = jrc::quest_offer_at(9330028, empty, novice, offer);
    check(!found, "quest 4014 needs quest 4013 in the STARTED state");

    Questlog ref_started;
    ref_started.add_started(4013, "");
    found = jrc::quest_offer_at(9330028, ref_started, novice, offer);
    check(same_offer(found, offer, 4014, false, false),
        "quest 4014 unlocks while quest 4013 is started");

    Questlog ref_done;
    ref_done.add_completed(4013, 1);
    found = jrc::quest_offer_at(9330028, ref_done, novice, offer);
    check(!found, "a completed prerequisite does not satisfy state 1");

    Questlog done_1000;
    done_1000.add_completed(1000, 1);
    found = jrc::quest_offer_at(2100, done_1000, novice, offer);
    check(same_offer(found, offer, 1001, false, false),
        "quest 1001 unlocks once quest 1000 is completed (state 2)");

    // --- interval cooldown: Check/0/interval minutes since completion
    // (quest 3205, npc 2040023, lvmin 20, interval 1440). ---
    const int64_t wall_now = 1800000000000LL;
    jrc::QuestCheckContext timed;
    timed.level = 25;
    timed.now_ms = wall_now;
    timed.item_count = novice.item_count;

    Questlog recent;
    recent.add_completed(3205, wall_now - 60LL * 60LL * 1000LL);
    found = jrc::quest_offer_at(2040023, recent, timed, offer);
    check(!found, "quest 3205 stays locked inside its 1440min interval");

    Questlog elapsed;
    elapsed.add_completed(3205, wall_now - 25LL * 60LL * 60LL * 1000LL);
    found = jrc::quest_offer_at(2040023, elapsed, timed, offer);
    check(same_offer(found, offer, 3205, false, false),
        "quest 3205 is re-offered after the interval elapsed");

    jrc::QuestCheckContext no_clock = timed;
    no_clock.now_ms = 0;
    found = jrc::quest_offer_at(2040023, recent, no_clock, offer);
    check(found && offer.quest_id == 3205,
        "without a wall clock the interval gate stays open (server "
        "remains authoritative)");

    if (failures == 0)
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
