#include <cassert>
#include <vector>

#include "Character/Look/Afterimage/Playback.h"
#include "Character/Look/Afterimage/Resolver.h"

int main()
{
    using jrc::afterimage::PlaybackPhase;

    const std::vector<int16_t> tiers{ 10, 0, 5, 10 };
    assert(jrc::afterimage::select_level_bucket(0, tiers) == 0);
    assert(jrc::afterimage::select_level_bucket(7, tiers) == 5);
    assert(jrc::afterimage::select_level_bucket(12, tiers) == 10);
    assert(jrc::afterimage::select_level_bucket(-1, tiers) == 0);
    assert(!jrc::afterimage::select_level_bucket(0, {}));

    PlaybackPhase phase = PlaybackPhase::WAITING_FOR_TRIGGER;
    phase = jrc::afterimage::latch_trigger(phase, false);
    assert(phase == PlaybackPhase::WAITING_FOR_TRIGGER);

    phase = jrc::afterimage::latch_trigger(phase, true);
    assert(phase == PlaybackPhase::WAITING_FOR_ASSETS);
    assert(!jrc::afterimage::should_draw(phase, false, false));

    // Swing cues can load after the stance has already returned to frame 0.
    // The latched state must still begin and draw instead of losing the slash.
    phase = jrc::afterimage::begin_when_ready(phase, false);
    assert(phase == PlaybackPhase::WAITING_FOR_ASSETS);
    assert(jrc::afterimage::should_draw(phase, true, false));

    PlaybackPhase phase_before_assets = phase;
    phase = jrc::afterimage::begin_when_ready(phase, true);
    assert(phase == PlaybackPhase::PLAYING);
    // A resident one-frame slash must survive the update that starts it so
    // the draw pass can present it at least once.
    assert(!jrc::afterimage::should_advance(
        phase_before_assets,
        phase,
        true,
        false
    ));
    // Updates cannot consume a resident one-frame slash until a draw pass has
    // actually submitted it.
    assert(!jrc::afterimage::should_advance(phase, phase, true, false));
    assert(jrc::afterimage::should_advance(phase, phase, true, true));
    phase = jrc::afterimage::finish(phase, false);
    assert(phase == PlaybackPhase::PLAYING);
    assert(jrc::afterimage::should_draw(phase, true, false));

    phase = jrc::afterimage::finish(phase, true);
    assert(phase == PlaybackPhase::COMPLETE);
    assert(!jrc::afterimage::should_draw(phase, true, true));

    // A bitmap can become resident between update and draw. The first frame
    // is drawable immediately, before the next update changes the phase.
    assert(jrc::afterimage::should_draw(
        PlaybackPhase::WAITING_FOR_TRIGGER,
        true,
        true
    ));

    // Trigger observation happens after the stance update. The previous frame
    // still closes the terminal-frame transition if the body wrapped to zero.
    assert(jrc::afterimage::reached_trigger(1, 2, 2));
    assert(jrc::afterimage::reached_trigger(2, 0, 2));
    assert(!jrc::afterimage::reached_trigger(0, 1, 2));

    // Starting another attack resets presentation independently of asset
    // readiness; the copied template must earn a new draw before advancing.
    phase = PlaybackPhase::WAITING_FOR_TRIGGER;
    phase = jrc::afterimage::latch_trigger(phase, true);
    phase_before_assets = phase;
    phase = jrc::afterimage::begin_when_ready(phase, true);
    assert(!jrc::afterimage::should_advance(
        phase_before_assets,
        phase,
        true,
        false
    ));
}
