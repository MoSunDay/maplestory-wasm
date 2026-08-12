#pragma once

#include "../../Graphics/Animation.h"
#include "../../Graphics/Texture.h"

#include "nlnx/node.hpp"

namespace jrc
{
    /// Owns the standard character death effect from Effect.nx/Tomb.img.
    class DeathTomb
    {
    public:
        /// Load the shared immutable tomb frames after NX initialization.
        static void initialize(nl::node source);

        /// Start the falling animation from its first frame.
        void start();
        /// Advance the fall-to-land state machine.
        void update();
        /// Draw the active fall animation or persistent landed frame.
        void draw(Point<int16_t> position, float alpha) const;
        /// Hide the tomb and discard any pending landed event.
        void clear();

        /// Return true only on the update that transitions to the landed frame.
        bool landed_this_update() const;

    private:
        enum class Phase
        {
            HIDDEN,
            FALLING,
            LANDED
        };

        static Animation fall_source;
        static Texture land_source;

        Animation fall;
        Phase phase = Phase::HIDDEN;
        bool landed_event = false;
    };
}
