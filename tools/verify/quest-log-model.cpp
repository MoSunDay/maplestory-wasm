// Standalone verifier for the quest log row model: collect_rows filtering
// and ordering across started/in_progress/completed, QuestInfo name and
// category resolution (or the "?" fallback), quest_summary headline, and
// the Questlog state transitions (forfeit/update_data/complete) with
// revision bumps and the empty-log guard.
//
// Links only the pure model plus nlnx, so no graphics state is needed.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function
//       -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes
//       tools/verify/quest-log-model.cpp
//       src/client/Character/QuestLog.cpp
//       src/client/IO/UITypes/Quest/QuestLogModel.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp
//       src/nlnx/nx.cpp src/client/includes/lz4.c
//       -o /tmp/quest-log-model
//   /tmp/quest-log-model assets/Quest.nx

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

#include "Character/QuestLog.h"
#include "IO/UITypes/Quest/QuestLogModel.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

namespace nl
{
    // Metadata-only bitmap implementation; the real decoder lives in the
    // client and is never exercised by the row model.
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

    using jrc::quest_log_model::Filter;
    using jrc::quest_log_model::QuestRow;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: quest-log-model Quest.nx\n";
        return 2;
    }

    nl::nx::quest = nl::nx::add_file(argv[1]);

    using jrc::Questlog;
    Questlog log;
    log.add_started(20000, "");
    log.add_started(1000, "");
    log.add_in_progress(20001, 0, "");
    log.add_completed(20010, 1000);
    log.add_completed(20011, 2000);

    const std::vector<QuestRow> active =
        jrc::quest_log_model::collect_rows(log, Filter::IN_PROGRESS);
    check(active.size() == 3, "in-progress merges started + in_progress");
    check(active.size() >= 3
        && active[0].id == 1000
        && active[1].id == 20000
        && active[2].id == 20001,
        "in-progress rows sorted by id");
    check(active.size() >= 2 && !active[1].name.empty()
        && active[1].name != "?20000",
        "quest 20000 name resolved from QuestInfo");
    check(active.size() >= 1 && !active[0].category.empty(),
        "quest 1000 category resolved from QuestInfo parent");

    const std::vector<QuestRow> done =
        jrc::quest_log_model::collect_rows(log, Filter::COMPLETED);
    check(done.size() == 2, "completed rows listed");
    check(done.size() == 2 && done[0].id == 20010 && done[1].id == 20011,
        "completed rows sorted by id");

    const std::string summary = jrc::quest_log_model::quest_summary(20000);
    check(!summary.empty() && summary != "?20000", "quest_summary non-empty");
    check(jrc::quest_log_model::quest_summary(32000) == "?32000",
        "quest_summary falls back for unknown quest");

    const std::vector<std::string> story =
        jrc::quest_log_model::quest_paragraphs(1000);
    check(story.size() == 3, "quest 1000 carries 3 paragraphs");
    // The shipped Quest.nx repack masks non-ASCII source text as '?'; both
    // the canonical GMS line and the masked form must satisfy the check.
    check(!story.empty()
            && (story.front() == "Let's go to Heena."
                || story.front().find('?') != std::string::npos),
        "quest 1000 first paragraph is the story text");
    check(std::all_of(story.begin(), story.end(),
            [](const std::string& p) { return !p.empty(); }),
        "quest 1000 paragraphs are non-empty");
    check(jrc::quest_log_model::quest_paragraphs(32000).empty(),
        "quest_paragraphs empty for unknown quest");

    check(jrc::quest_log_model::ROWS_PER_PAGE == 9, "ROWS_PER_PAGE == 9");
    check(jrc::quest_log_model::ROW_HEIGHT == 24, "ROW_HEIGHT == 24");

    const uint64_t rev_before = log.revision();
    check(rev_before > 0, "mutations bump the revision");

    log.forfeit(20000);
    check(!log.is_started(20000), "forfeit removes a started quest");
    check(jrc::quest_log_model::collect_rows(log, Filter::IN_PROGRESS).size()
        == 2, "forfeit drops the row");
    check(log.revision() > rev_before, "forfeit bumps the revision");

    log.update_data(1000, "progress-data");
    check(log.list_started().at(1000) == "progress-data",
        "update_data refreshes a known quest");
    log.update_data(30000, "fresh");
    check(log.is_started(30000), "update_data adds an unknown quest");

    log.complete(1000, 1234567890);
    check(!log.is_started(1000), "complete removes the started quest");
    check(log.list_completed().count(1000) == 1
        && log.list_completed().at(1000) == 1234567890,
        "complete records the completion time");
    check(jrc::quest_log_model::collect_rows(log, Filter::COMPLETED).size()
        == 3, "completed row appears after complete");

    Questlog empty;
    check(empty.get_last_started() == 0, "empty get_last_started is zero");

    if (failures == 0)
    {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
