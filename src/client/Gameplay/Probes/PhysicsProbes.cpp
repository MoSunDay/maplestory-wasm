// Local-player physics probes for the e2e harnesses (snow slide / aqua swim).
// GameplayProbes.cpp is at its line budget, so player physics probes live in
// this dedicated translation unit following the same extern "C" conventions.
#include "../Stage.h"

#include <cstdint>

namespace
{
    const jrc::PhysicsObject& local_phobj()
    {
        return jrc::Stage::get().get_player().get_phobj();
    }
}

extern "C"
{
    // Horizontal speed in 1/100 px per frame; sign keeps the direction.
    int msplayer_hspeed()
    {
        return static_cast<int>(local_phobj().hspeed * 100.0);
    }

    // Vertical speed in 1/100 px per frame; negative is upward.
    int msplayer_vspeed()
    {
        return static_cast<int>(local_phobj().vspeed * 100.0);
    }

    // 1 while the player rests on a foothold, 0 while airborne/swimming.
    int msplayer_onground()
    {
        return local_phobj().onground ? 1 : 0;
    }

    // PhysicsObject::Type driving the local player
    // (NORMAL=0, ICE=1, SWIMMING=2, FLYING=3, FIXATED=4).
    int msplayer_physicstype()
    {
        return static_cast<int>(local_phobj().type);
    }
}
