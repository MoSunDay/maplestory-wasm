#pragma once

namespace jrc::afterimage
{
    enum class PlaybackPhase
    {
        WAITING_FOR_TRIGGER,
        WAITING_FOR_ASSETS,
        PLAYING,
        COMPLETE
    };

    inline PlaybackPhase latch_trigger(PlaybackPhase phase, bool reached)
    {
        if (phase == PlaybackPhase::WAITING_FOR_TRIGGER && reached)
        {
            return PlaybackPhase::WAITING_FOR_ASSETS;
        }
        return phase;
    }

    inline PlaybackPhase begin_when_ready(PlaybackPhase phase, bool ready)
    {
        if (phase == PlaybackPhase::WAITING_FOR_ASSETS && ready)
        {
            return PlaybackPhase::PLAYING;
        }
        return phase;
    }

    inline PlaybackPhase finish(PlaybackPhase phase, bool animation_ended)
    {
        if (phase == PlaybackPhase::PLAYING && animation_ended)
        {
            return PlaybackPhase::COMPLETE;
        }
        return phase;
    }

    inline bool should_advance(
        PlaybackPhase phase_before_assets,
        PlaybackPhase phase_after_assets,
        bool ready
    )
    {
        return ready &&
            phase_before_assets == PlaybackPhase::PLAYING &&
            phase_after_assets == PlaybackPhase::PLAYING;
    }

    inline bool should_draw(PlaybackPhase phase, bool ready, bool reached_now)
    {
        return ready && (
            phase == PlaybackPhase::PLAYING ||
            phase == PlaybackPhase::WAITING_FOR_ASSETS ||
            (phase == PlaybackPhase::WAITING_FOR_TRIGGER && reached_now)
        );
    }
}
