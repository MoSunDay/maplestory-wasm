// Read-only runtime probe for e2e drivers: exposes Combat's per-skill buff
// cast throttle state (ms timestamp of the last successful cast, 0 = never)
// so browser tests can assert recast suppression without packet capture.
#include "../Combat/Combat.h"
#include "../Stage.h"

extern "C" double mscombat_last_buffcast(int32_t skillid)
{
    return static_cast<double>(
        jrc::Stage::get().get_combat().probe_last_buffcast_ms(skillid)
    );
}
