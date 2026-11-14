// Remote pet movement replay, mirroring the PetLook call sites:
// - apply_movements anchors the batch on the first positioned fragment
//   (landing on its endpoint) and hands the remaining fragments to the
//   mobplayback channel, so a multi-fragment MOVE_PET no longer snaps to
//   the last fragment;
// - update steps Constants::TIMESTEP (8 ms) per frame, interpolating the
//   fragment endpoints across their duration, so an 80 ms fragment passes
//   its spatial midpoint at half its duration;
// - when the batch is exhausted the pet holds the final endpoint and the
//   playback channel is cleared, which is the hand-off signal back to the
//   local follow simulation.
// PetLook itself pulls in the Animation/NX stack, so the replay core is
// verified directly, like the char movement playback test.
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

    bool sample_at(const mobplayback::Sample& sample, int16_t x, int16_t y)
    {
        return sample.position.x() == x && sample.position.y() == y;
    }

    bool point_at(const Point<int16_t>& point, int16_t x, int16_t y)
    {
        return point.x() == x && point.y() == y;
    }

    mobplayback::Sample advance(mobplayback::State& state, int frames)
    {
        mobplayback::Sample sample{};
        for (int i = 0; i < frames; ++i)
            sample = mobplayback::step(state, Constants::TIMESTEP);
        return sample;
    }

    // PetLook::apply_movements: the first positioned fragment lands
    // directly (position, foothold, stance) and the rest form the replay
    // batch anchored at that endpoint.
    mobplayback::State apply_like_pet(const std::vector<Movement>& movements,
        Point<int16_t>& position)
    {
        const Movement& first = movements.front();
        position = { first.xpos, first.ypos };
        return mobplayback::begin(position,
            { movements.begin() + 1, movements.end() });
    }
}

int main()
{
    using namespace jrc::mobplayback;

    Point<int16_t> position{};
    // PetLook::apply_movements hands the trailing fragments to the replay
    // channel anchored at the first endpoint.
    std::vector<Movement> batch;
    batch.push_back(absolute(180, 100, 100, 100, 1, 1, 80));
    batch.push_back(absolute(260, 100, 180, 100, 2, 3, 80));
    State playback = apply_like_pet(batch, position);
    require(playback.started(), "the trailing fragment starts a replay");
    require(position.x() == 180 && position.y() == 100,
        "the batch is anchored on the first fragment endpoint");

    // Half of the 80 ms fragment (5 frames of 8 ms) reaches the spatial
    // midpoint between the anchor and the fragment endpoint.
    const Sample middle = advance(playback, 5);
    require(sample_at(middle, 220, 100) && !middle.done && middle.state == 3,
        "80 ms fragment passes its midpoint at half its duration");

    // Exhausting the fragment lands exactly on the endpoint and holds it.
    const Sample landed = advance(playback, 5);
    require(sample_at(landed, 260, 100) && landed.done && landed.state == 3
        && landed.fh == 2,
        "exhausted batch stops on the final endpoint");

    // PetLook clears the channel on 'done'; the held endpoint stays the
    // pet position and the empty channel reports done every frame, which
    // is the hand-off signal back to the local follow simulation.
    playback = State{};
    const Sample handoff = step(playback, Constants::TIMESTEP);
    require(handoff.done && !playback.started()
        && point_at(landed.position, 260, 100),
        "cleared channel is idle so local following resumes from the endpoint");

    // A single-fragment batch never starts a replay: the anchor snap is
    // the whole packet, matching the pre-playback behaviour.
    std::vector<Movement> single;
    single.push_back(absolute(340, 60, 260, 100, 4, 5, 60));
    Point<int16_t> snapped{};
    const State instant = apply_like_pet(single, snapped);
    require(!instant.started() && point_at(snapped, 340, 60),
        "single-fragment batch lands directly without a replay");

    // An empty batch never moves the pet.
    State idle = begin({ 340, 60 }, {});
    const Sample standing = advance(idle, 4);
    require(sample_at(standing, 340, 60) && standing.done,
        "empty batch holds the pet in place");

    std::cout << "pet playback interpolation verification passed\n";
}
