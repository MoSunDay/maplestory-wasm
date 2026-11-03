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

    // Walk 100->180 over 80 ms on fh 3 (stance 3 = move, unflipped), then
    // hop to (180, 60) over 40 ms on fh 5 (stance 7 = jump, unflipped).
    std::vector<Movement> batch;
    batch.push_back(absolute(180, 100, 100, 100, 3, 3, 80));
    batch.push_back(relative(180, 60, 7, 40));
    State state = begin({ 100, 100 }, std::move(batch));

    const Sample first = advance(state, 1);
    require(sample_at(first, 108, 100),
            "first frame interpolates one tenth into fragment one");
    require(first.fh == 3 && first.state == 3 && !first.done,
            "first frame carries fragment one fh and stance");

    const Sample crossover = advance(state, 9);
    require(sample_at(crossover, 180, 100),
            "second fragment starts at the first fragment endpoint");
    require(crossover.fh == 0 && crossover.state == 7 && !crossover.done,
            "relative fragment carries stance but no fh");

    const Sample end = advance(state, 5);
    require(sample_at(end, 180, 60) && end.done,
            "exhausted batch holds the final endpoint");
    require(end.state == 7 && end.fh == 0,
            "hold keeps the final fragment stance and fh");

    const Sample held = advance(state, 3);
    require(sample_at(held, 180, 60) && held.done, "hold is stable");

    // The controlling client sends one duration-1 fragment per packet:
    // it must land on the endpoint immediately and then hold.
    std::vector<Movement> single;
    single.push_back(absolute(200, 100, 100, 100, 2, 5, 1));
    State instant = begin({ 100, 100 }, std::move(single));
    const Sample snap = advance(instant, 1);
    require(sample_at(snap, 200, 100) && snap.done && snap.state == 5,
            "duration-1 fragment snaps and holds");

    // Playback re-anchors at the packet start position even when the
    // fragment claims a different 'last' position.
    std::vector<Movement> anchored;
    anchored.push_back(absolute(300, 100, 999, 999, 1, 2, 100));
    State reanchored = begin({ 200, 100 }, std::move(anchored));
    const Sample origin = advance(reanchored, 1);
    require(sample_at(origin, 208, 100),
            "first fragment interpolates from the server start");

    // Unpositioned fragments (teleports, equip changes) add no time and
    // never become waypoints.
    std::vector<Movement> gapped;
    gapped.push_back(absolute(150, 100, 100, 100, 3, 3, 40));
    gapped.push_back(Movement{ Movement::NONE, 14, 0, 0, 0, 0, 0, 2, 50 });
    gapped.push_back(relative(150, 80, 7, 40));
    State skipped = begin({ 100, 100 }, std::move(gapped));
    const Sample past = advance(skipped, 5);
    require(sample_at(past, 150, 100) && past.state == 7 && past.fh == 0,
            "unpositioned fragment is skipped without a zero sweep");

    const Sample finish = advance(skipped, 5);
    require(sample_at(finish, 150, 80) && finish.done,
            "batch after skipped fragment still completes");

    // An empty batch never moves the mob; callers keep the spawn position.
    State empty = begin({ 50, 60 }, {});
    const Sample none = advance(empty, 4);
    require(sample_at(none, 50, 60) && none.done && none.state == 0,
            "empty batch reports done at the start");

    std::cout << "mob movement playback verification passed\n";
}
