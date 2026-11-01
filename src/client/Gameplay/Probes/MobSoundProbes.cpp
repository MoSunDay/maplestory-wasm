// Read-only runtime probe for e2e drivers: exposes mob attack/skill sound
// playback counters (see MapleMap/MobSoundStats.h) so browser tests can
// assert that attacking mobs trigger sound playback.
#include "../MapleMap/MobSoundStats.h"
#include "../Stage.h"

extern "C"
{
    int msmob_sound_played_count()
    {
        return jrc::mobsound::played_count().load(std::memory_order_relaxed);
    }

    int msmob_sound_loaded_count()
    {
        return jrc::mobsound::loaded_count().load(std::memory_order_relaxed);
    }

    int msmob_sound_last_mob()
    {
        return jrc::mobsound::last_played_mob().load(std::memory_order_relaxed);
    }

    int msmob_sound_last_action()
    {
        return jrc::mobsound::last_played_action().load(std::memory_order_relaxed);
    }

    int msmob_sound_last_skill()
    {
        return jrc::mobsound::last_played_skill().load(std::memory_order_relaxed);
    }

    int msmob_sound_probe_play()
    {
        return jrc::Stage::get().get_mobs().probe_play_mob_sound() ? 1 : 0;
    }
}
