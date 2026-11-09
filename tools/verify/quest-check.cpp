// Standalone verifier for the QuestCheck module: the Check.img reverse
// npc index, the script/autoStart/interval flags, QuestInfo names, and the
// accept/deliver offer resolution (mob progress decoding plus item stubs)
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

    if (failures == 0)
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
