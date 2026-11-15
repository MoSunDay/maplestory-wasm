// Remote summon pool probes: browser-side e2e samples the live summon
// count and the skill of the oid-sorted i-th entity to assert that a
// departing owner leaves no ghost summons behind.
#include "../Stage.h"
#include "../Summon/SummonPool.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C"
{
    int ms_summon_count()
    {
        return static_cast<int>(
            jrc::summon::size(jrc::Stage::get().get_summons()));
    }

    int ms_summon_skill_at(int index)
    {
        jrc::summon::SummonPool& pool = jrc::Stage::get().get_summons();
        std::vector<int32_t> oids;
        oids.reserve(jrc::summon::size(pool));
        for (const auto& entry : pool.entities)
            oids.push_back(entry.first);
        std::sort(oids.begin(), oids.end());

        if (index < 0
            || static_cast<size_t>(index) >= oids.size())
            return -1;

        const jrc::summon::SummonEntity* entity =
            jrc::summon::find(pool, oids[index]);
        return entity ? entity->skillid : -1;
    }
}
