#include "JobProgression.h"

namespace jrc::job_progression
{
    Stage stage_for(uint16_t job_id)
    {
        if (job_id == beginner_job_for(job_id))
            return Stage::BEGINNER;
        if (job_id % 100 == 0)
            return Stage::FIRST;
        if (job_id % 10 == 0)
            return Stage::SECOND;
        if (job_id % 10 == 1)
            return Stage::THIRD;
        return Stage::FOURTH;
    }

    uint16_t beginner_job_for(uint16_t job_id)
    {
        if (job_id >= 2000 && job_id < 3000)
            return 2000;
        if (job_id >= 1000 && job_id < 2000)
            return 1000;
        return 0;
    }

    uint16_t job_for_stage(uint16_t job_id, Stage stage)
    {
        if (stage > stage_for(job_id))
            return 0;

        switch (stage)
        {
        case Stage::BEGINNER:
            return beginner_job_for(job_id);
        case Stage::FIRST:
            return static_cast<uint16_t>((job_id / 100) * 100);
        case Stage::SECOND:
            return static_cast<uint16_t>((job_id / 10) * 10);
        case Stage::THIRD:
            return stage_for(job_id) == Stage::FOURTH ? job_id - 1 : job_id;
        case Stage::FOURTH:
            return job_id;
        }
        return 0;
    }

    int32_t beginner_skill_for(uint16_t job_id, uint8_t index)
    {
        return static_cast<int32_t>(beginner_job_for(job_id)) * 10000 + 1000 + index;
    }

    uint8_t archetype_for(uint16_t job_id)
    {
        if (job_id >= 2000 && job_id < 3000)
            return 1;
        if (job_id >= 1000 && job_id < 2000)
            return static_cast<uint8_t>((job_id - 1000) / 100);
        return static_cast<uint8_t>(job_id / 100);
    }
}
