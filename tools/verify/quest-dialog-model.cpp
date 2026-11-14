// Standalone verifier for QuestDialogModel: the NPC speech pages
// (Say.img), the QuestInfo summary fallback, the Chinese last-resort
// lines, and the prop == -1 selectable-reward table (Act.img/<qid>/1)
// whose numeric child order must match the server's selectList indices.
// Data facts cross-checked with link_repos/MapleStory-Server/wz/Quest.wz/
// Say.img.xml, QuestInfo.img.xml and Act.img.xml.
//
// Links only the pure model plus nlnx, so no graphics state is needed.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function
//       -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes
//       tools/verify/quest-dialog-model.cpp
//       src/client/IO/UITypes/Quest/QuestDialogModel.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp
//       src/nlnx/nx.cpp src/client/includes/lz4.c
//       -o /tmp/quest-dialog-model
//   /tmp/quest-dialog-model assets/Quest.nx

#include <cstdint>
#include <iostream>
#include <string>

#include "IO/UITypes/Quest/QuestDialogModel.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

namespace nl
{
    // Metadata-only bitmap implementation; the real decoder lives in the
    // client and is never exercised by the dialog model.
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

    bool same_choice(const jrc::quest_dialog_model::RewardChoice& choice,
        int32_t itemid, int32_t count)
    {
        return choice.itemid == itemid && choice.count == count;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: quest-dialog-model Quest.nx\n";
        return 2;
    }

    nl::nx::quest = nl::nx::add_file(argv[1]);

    using jrc::quest_dialog_model::collect;
    using jrc::quest_dialog_model::selectable_rewards;

    // --- accept speech: Say.img/<qid>/0 numbered pages, verbatim macros ---
    const auto sera = collect(1000, false);
    check(sera.title == "Borrowing Sera's Mirror",
        "quest 1000 title comes from QuestInfo.img/name");
    check(sera.pages.size() == 3,
        "quest 1000 accept speech has three pages");
    check(!sera.pages.empty()
        && sera.pages[0].rfind("You must be the new traveler", 0) == 0,
        "quest 1000 first page is the Say.img/0/0 speech");
    check(sera.decline_page.rfind("Don't want to?", 0) == 0,
        "quest 1000 decline page comes from Say.img/0/no/0");

    // --- delivery speech with a Say.img/1 page ---
    const auto mirror = collect(1001, true);
    check(mirror.pages.size() == 1,
        "quest 1001 delivery speech has one page");
    check(!mirror.pages.empty()
        && mirror.pages[0].rfind("Oh wow! You brought", 0) == 0,
        "quest 1001 delivery page is the Say.img/1/0 speech");
    check(mirror.decline_page.empty(),
        "quest 1001 ships no delivery decline page");

    // --- Say-less stage borrows the QuestInfo summary paragraph ---
    const auto summary = collect(1000, true);
    check(summary.pages.size() == 1,
        "quest 1000 delivery stage has no Say pages, so one summary remains");
    check(!summary.pages.empty()
        && summary.pages[0].rfind("I ran into Heena", 0) == 0,
        "quest 1000 delivery page falls back to QuestInfo.img/1");

    // --- no Say and no summary: Chinese last-resort lines ---
    const auto bare = collect(10107, false);
    check(bare.pages.size() == 1 && bare.pages[0] == "你愿意接受这个任务吗？",
        "quest 10107 keeps the Chinese accept fallback");
    const auto bare_deliver = collect(10107, true);
    check(bare_deliver.pages.size() == 1
        && bare_deliver.pages[0] == "确定要提交这个任务吗？",
        "quest 10107 keeps the Chinese delivery fallback");

    // --- unknown quest: fallback title and page ---
    const auto unknown = collect(32000, false);
    check(unknown.title == "任务", "unknown quest title falls back to 任务");
    check(unknown.pages.size() == 1
        && unknown.pages[0] == "你愿意接受这个任务吗？",
        "unknown quest keeps the fallback page");

    // --- selectable rewards: only prop == -1, numeric child order ---
    const auto gladius = selectable_rewards(2047);
    check(gladius.size() == 2, "quest 2047 offers exactly two choices");
    check(gladius.size() == 2
        && same_choice(gladius[0], 1302015, 1)
        && same_choice(gladius[1], 1032012, 1),
        "quest 2047 choices keep the server selectList order");

    const auto random_box = selectable_rewards(2094);
    check(random_box.size() == 16,
        "quest 2094 exposes all sixteen prop=-1 rewards");
    check(random_box.size() == 16
        && same_choice(random_box.front(), 2043302, 1)
        && same_choice(random_box.back(), 2044802, 1),
        "quest 2094 reward order follows numeric node names");

    const auto tickets = selectable_rewards(10272);
    check(tickets.size() == 2
        && same_choice(tickets[0], 4032283, -10)
        && same_choice(tickets[1], 4032280, -10),
        "quest 10272 keeps negative counts (take-item selections)");

    check(selectable_rewards(1000).empty(),
        "quest 1000 ships no selectable rewards");
    check(selectable_rewards(1201).empty(),
        "quest 1201 ships no selectable rewards");

    // --- determinism: repeated calls return identical content ---
    const auto again = selectable_rewards(2047);
    check(again.size() == gladius.size(),
        "selectable_rewards is deterministic across calls");
    bool deterministic = again.size() == gladius.size();
    for (size_t i = 0; deterministic && i < again.size(); ++i)
    {
        deterministic = same_choice(again[i], gladius[i].itemid, gladius[i].count);
    }
    check(deterministic, "selectable_rewards repeats the same choice table");

    if (failures == 0)
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
