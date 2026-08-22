#pragma once

#include <cstdint>

namespace jrc::job_progression
{
    enum class Stage : uint16_t
    {
        BEGINNER,
        FIRST,
        SECOND,
        THIRD,
        FOURTH
    };

    Stage stage_for(uint16_t job_id);
    uint16_t beginner_job_for(uint16_t job_id);
    uint16_t job_for_stage(uint16_t job_id, Stage stage);
    uint8_t archetype_for(uint16_t job_id);
    int32_t beginner_skill_for(uint16_t job_id, uint8_t index);
}
