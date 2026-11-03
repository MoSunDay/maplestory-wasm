// Remote player movement replay, mirroring the OtherChar call sites:
// - send_movement anchors a batch at the wire 'last' position of its first
//   positioned fragment (MOVE_PLAYER carries no explicit start) and drops
//   any batch still replaying;
// - update steps 8 ms per frame, interpolating fragment endpoints by
//   duration and switching stance on fragment change;
// - a drained batch holds its endpoint instead of teleporting;
// - respawn (repeated SPAWN_PLAYER) re-anchors with no fragments.
// OtherChar itself pulls in the CharLook/Stage/NX stack, so the replay core
// is verified directly, as with the mob playback test.
#include "Gameplay/MapleMap/MobMovementPlayback.h"

#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << message << '\n';
            std::exit(1);
        }
    }

    using namespace jrc;

    Movement absolute(int16_t x, int16_t y, int16_t lx, int16_t ly,
                      uint16_t fh, uint8_t state, int16_t duration)
    {
        return { Movement::_ABSOLUTE, 0, x, y, lx, ly, fh, state, duration };
    }

    Movement relative(int16_t x, int16_t y, uint8_t state, int16_t duration)
    {
        Movement fragment{ Movement::NONE, 1, 0, 0, 0, 0, 0, 0, 0 };
        fragment.type = Movement::_RELATIVE;
        fragment.xpos = x;
        fragment.ypos = y;
        fragment.newstate = state;
        fragment.duration = duration;
        return fragment;
    }

    bool sample_at(const mobplayback::Sample& sample, int16_t x, int16_t y)
    {
        return sample.position.x() == x && sample.position.y() == y;
    }

    mobplayback::Sample advance(mobplayback::State& state, int frames)
    {
        mobplayback::Sample sample{};
        for (int i = 0; i < frames; ++i)
            sample = mobplayback::step(state, 8.0);
        return sample;
    }
}

int main()
{
    using namespace jrc::mobplayback;

    // Walk (100,100)->(180,100) over 80 ms in stance 2 (WALK, facing right),
    // then fall to (180,60) over 40 ms in stance 6 (FALL). The batch is
    // anchored at the first fragment's wire 'last' position, as
    // OtherChar::send_movement does for MOVE_PLAYER.
    std::vector<Movement> batch;
    batch.push_back(absolute(180, 100, 100, 100, 3, 2, 80));
    batch.push_back(relative(180, 60, 6, 40));
    State state = begin({ 100, 100 }, std::move(batch));

    const Sample first = advance(state, 1);
    require(sample_at(first, 108, 100) && first.state == 2 && !first.done,
            "first frame interpolates one tenth into the walk fragment");

    const Sample second = advance(state, 1);
    require(sample_at(second, 116, 100) && second.state == 2,
            "interpolation advances linearly with the 8 ms steps");

    const Sample crossover = advance(state, 8);
    require(sample_at(crossover, 180, 100),
            "second fragment starts at the walk fragment endpoint");
    require(crossover.state == 6 && !crossover.done,
            "fragment switch applies the new stance immediately");

    const Sample end = advance(state, 5);
    require(sample_at(end, 180, 60) && end.done,
            "drained batch holds the final endpoint instead of teleporting");

    const Sample held = advance(state, 3);
    require(sample_at(held, 180, 60) && held.state == 6 && held.done,
            "the hold keeps the last stance and is stable");

    // A batch whose first fragment carries no wire 'last' position is
    // anchored at the position the replay currently holds.
    std::vector<Movement> relfirst;
    relfirst.push_back(relative(150, 100, 2, 40));
    State kept = begin({ 120, 100 }, std::move(relfirst));
    const Sample keptstart = advance(kept, 1);
    require(sample_at(keptstart, 126, 100),
            "relative-first batch interpolates from the held position");
    const Sample keptend = advance(kept, 4);
    require(sample_at(keptend, 150, 100) && keptend.done,
            "relative-first batch still completes");

    // A new packet replaces a batch that is still replaying: no time or
    // waypoint leaks in from the dropped fragments.
    std::vector<Movement> stale;
    stale.push_back(absolute(900, 100, 100, 100, 1, 2, 1000));
    State replaced = begin({ 100, 100 }, std::move(stale));
    advance(replaced, 3);
    std::vector<Movement> fresh;
    fresh.push_back(absolute(300, 100, 200, 100, 1, 4, 80));
    replaced = begin({ 200, 100 }, std::move(fresh));
    const Sample restart = advance(replaced, 1);
    require(sample_at(restart, 210, 100) && restart.state == 4,
            "new batch re-anchors at its own start and replaces the old one");

    // The controlling client sends one duration-1 fragment per packet while
    // moving: it lands on the endpoint immediately and then holds.
    std::vector<Movement> single;
    single.push_back(absolute(140, 100, 100, 100, 2, 2, 1));
    State instant = begin({ 100, 100 }, std::move(single));
    const Sample snap = advance(instant, 1);
    require(sample_at(snap, 140, 100) && snap.done && snap.state == 2,
            "duration-1 fragment snaps to its endpoint and holds");

    // A repeated SPAWN_PLAYER is an authoritative correction: the replay
    // is re-anchored without fragments, so the character freezes at the
    // packet position and no stale fragment can pull it back.
    std::vector<Movement> walking;
    walking.push_back(absolute(500, 100, 100, 100, 2, 2, 800));
    State respawned = begin({ 100, 100 }, std::move(walking));
    advance(respawned, 5);
    respawned = begin({ 260, 40 }, {});
    const Sample corrected = advance(respawned, 1);
    require(sample_at(corrected, 260, 40) && corrected.done,
            "respawn drops fragments and holds the packet position");
    require(corrected.state == 0,
            "respawn leaves the stance to the packet, not old fragments");
    const Sample again = advance(respawned, 6);
    require(sample_at(again, 260, 40) && again.done,
            "the refresh is idempotent");

    // A spawned character without movement packets stays where it spawned.
    State standing = begin({ 50, 60 }, {});
    const Sample none = advance(standing, 4);
    require(sample_at(none, 50, 60) && none.done,
            "empty batch never moves the character");

    std::cout << "char movement playback verification passed\n";
}
