#pragma once

#include "../../Graphics/Animation.h"
#include "../../Graphics/Texture.h"
#include "../../Template/Interpolated.h"

#include "nlnx/node.hpp"

namespace jrc
{
    class Physics;

    /// Owns the standard character death effect from Effect.nx/Tomb.img.
    class DeathTomb
    {
    public:
        /// Load the shared immutable tomb frames after NX initialization.
        static void initialize(nl::node source);

        /// Start the falling animation at the character's death position.
        void start(Point<int16_t> death_position);
        /// Resolve the ground anchor once and advance the animation state.
        void update(const Physics& physics);
        /// Draw the active fall animation or persistent landed frame.
        void draw(Point<int16_t> position, float alpha) const;
        /// Return the fixed tomb anchor projected through the current camera.
        Point<int16_t> get_absolute(double view_x, double view_y) const;
        /// Return the interpolated ghost offset around the landed tomb.
        Point<int16_t> ghost_offset(float alpha) const;
        /// Return whether the ghost belongs in front of the landed tomb.
        bool ghost_in_front(float alpha) const;
        /// Hide the tomb and discard any pending landed event.
        void clear();

        /// Return true only on the update that transitions to the landed frame.
        bool landed_this_update() const;
        /// Return whether the persistent landed frame is active.
        bool is_landed() const;

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
        Linear<float> orbit_angle;
        Point<int16_t> death_position;
        Point<int16_t> ground_anchor;
        Phase phase = Phase::HIDDEN;
        bool ground_resolved = false;
        bool landed_event = false;
    };
}
