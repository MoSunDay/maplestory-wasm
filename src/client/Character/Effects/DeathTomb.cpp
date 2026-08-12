#include "DeathTomb.h"

namespace jrc
{
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

    void DeathTomb::clear()
    {
        phase = Phase::HIDDEN;
        landed_event = false;
    }

    bool DeathTomb::landed_this_update() const
    {
        return landed_event;
    }
}
