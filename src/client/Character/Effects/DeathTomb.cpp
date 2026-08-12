#include "DeathTomb.h"
#include "DeathTombGround.h"
#include "DeathTombOrbit.h"

#include "../../Constants.h"
#include "../../Gameplay/Physics/Physics.h"

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

    void DeathTomb::start(Point<int16_t> position)
    {
        fall = fall_source;
        fall.reset();
        orbit_angle.set(0.0f);
        death_position = position;
        ground_anchor = position;
        phase = Phase::FALLING;
        ground_resolved = false;
        landed_event = false;
    }

    void DeathTomb::update(const Physics& physics)
    {
        landed_event = false;
        if (phase == Phase::HIDDEN)
        {
            return;
        }

        if (!ground_resolved)
        {
            ground_anchor = death_tomb_ground::landing_anchor(
                death_position,
                physics.get_y_below(death_position)
            );
            ground_resolved = true;
        }

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

    Point<int16_t> DeathTomb::get_absolute(double view_x, double view_y) const
    {
        return death_tomb_ground::absolute_position(
            ground_anchor,
            view_x,
            view_y
        );
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
        death_position = {};
        ground_anchor = {};
        ground_resolved = false;
        landed_event = false;
    }

    bool DeathTomb::landed_this_update() const
    {
        return landed_event;
    }

    bool DeathTomb::is_landed() const
    {
        return phase == Phase::LANDED;
    }
}
