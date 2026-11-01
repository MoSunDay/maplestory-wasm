//////////////////////////////////////////////////////////////////////////////
// Pure row model for the quest log window.
//
// Deliberately nx-free so the filters can be linked and tested standalone
// (tools/verify/quest-log-model.cpp); the nx lookups live in the .cpp.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../../Character/QuestLog.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jrc::quest_log_model
{
    // One row of the quest list.
    struct QuestRow
    {
        int16_t id = 0;
        std::string name;
        std::string category;
        // Raw quest data ("3/10" mob progress and the like) while the
        // quest runs; empty when the server sent none.
        std::string progress;
        // Completion time of finished quests; 0 while the quest runs.
        int64_t completed_time = 0;
    };

    // Which half of the log the left pane lists.
    enum class Filter : int8_t
    {
        IN_PROGRESS = 0,
        COMPLETED = 1
    };

    // Rows for one filter, sorted by quest id. Names and categories come
    // from the QuestInfo nx data; unknown quests fall back to "?" + id.
    std::vector<QuestRow> collect_rows(const Questlog& log, Filter filter);

    // Short headline for the info pane: category + name, no long state
    // dialogues.
    std::string quest_summary(int16_t id);

    // Story paragraphs for the info pane: the numbered string children
    // ("0", "1", ...) of the QuestInfo entry, in order. Empty for unknown
    // quests.
    std::vector<std::string> quest_paragraphs(int16_t id);

    // Completion date "YYYY-MM-DD" (UTC) for the info pane. Accepts both
    // the setfield FILETIME and plain epoch seconds; "?" when unusable.
    std::string format_completed_time(int64_t time);

    constexpr size_t ROWS_PER_PAGE = 9;
    constexpr int16_t ROW_HEIGHT = 24;
}
