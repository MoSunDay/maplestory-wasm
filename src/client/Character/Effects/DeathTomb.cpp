#include "DeathTomb.h"
#include "DeathTombOrbit.h"

#include "../../Constants.h"

namespace jrc
{
    namespace
    {
        constexpr float FULL_TURN = 6.28318530718f;
        constexpr float ORBIT_STEP =
            FULL_TURN * Constants::TIMESTEP / 2400.0f;
    }

    Animation DeathTomb::fall_source;
    Texture DeathTomb::land_source;

    void DeathTomb::initialize(nl::node source)
    {
        fall_source = Animation(source["fall"]);
        land_source = Texture(source["land"]["0"]);
    }

    void DeathTomb::start()
    {
        fall = fall_source;
        fall.reset();
        orbit_angle.set(0.0f);
        phase = Phase::FALLING;
        landed_event = false;
    }

    void DeathTomb::update()
    {
        landed_event = false;
        if (phase == Phase::FALLING && fall.update())
        {
            phase = Phase::LANDED;
            landed_event = true;
        }
        else if (phase == Phase::LANDED)
        {
            orbit_angle += ORBIT_STEP;
        }
    }

    void DeathTomb::draw(Point<int16_t> position, float alpha) const
    {
        if (phase == Phase::FALLING)
        {
            fall.draw(position, alpha);
        }
        else if (phase == Phase::LANDED)
        {
            land_source.draw(position);
        }
    }

    Point<int16_t> DeathTomb::ghost_offset(float alpha) const
    {
        if (phase != Phase::LANDED)
        {
            return {};
        }

        return death_orbit::offset(orbit_angle.get(alpha));
    }

    bool DeathTomb::ghost_in_front(float alpha) const
    {
        if (phase != Phase::LANDED)
        {
            return false;
        }

        return death_orbit::in_front(orbit_angle.get(alpha));
    }

    void DeathTomb::clear()
    {
        phase = Phase::HIDDEN;
        orbit_angle.set(0.0f);
        landed_event = false;
    }

    bool DeathTomb::landed_this_update() const
    {
        return landed_event;
    }
}
