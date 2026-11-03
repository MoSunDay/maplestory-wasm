//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../Movement.h"

#include "../../Template/Point.h"
#include "../../Util/Lerp.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace jrc
{
    /// Replay of server movement fragments for non-controlled mobs.
    /// Plain data and free functions only; the caller owns the state.
    namespace mobplayback
    {
        /// Activity bytes below this are plain stances; at or above it they
        /// are attack/skill activities handled outside of playback.
        constexpr uint8_t ACTIVITY_FIRST = 24;

        /// Where a mob should be for one frame, derived from fragments.
        struct Sample
        {
            Point<int16_t> position;
            uint16_t fh = 0;
            uint8_t state = 0;
            bool done = true;
        };

        /// Cursor over one fragment batch, anchored at the server start.
        struct State
        {
            std::vector<Movement> fragments;
            Point<int16_t> start;
            size_t index = 0;
            double elapsed = 0.0;

            bool started() const
            {
                return !fragments.empty();
            }
        };

        /// Anchor a new batch at the server-provided start position.
        inline State begin(Point<int16_t> start, std::vector<Movement>&& fragments)
        {
            return State{ std::move(fragments), start, 0, 0.0 };
        }

        /// Whether a fragment carries usable position data. Parsed
        /// fragments without positions (teleport, equip change) contribute
        /// no time and no waypoint to the replay.
        inline bool positioned(const Movement& fragment)
        {
            return fragment.type == Movement::_ABSOLUTE
                || fragment.type == Movement::_RELATIVE
                || fragment.type == Movement::CHAIR
                || fragment.type == Movement::JUMPDOWN;
        }

        /// Segment origin of the positioned fragment at 'index': the batch
        /// start for the first one, otherwise the previous positioned
        /// fragment's endpoint. Chaining endpoints keeps the path continuous
        /// even when the wire 'last' fields of a fragment carry velocity
        /// instead of a position.
        inline Point<int16_t> origin_of(const State& state, size_t index)
        {
            while (index > 0)
            {
                const Movement& previous = state.fragments[index - 1];
                if (positioned(previous))
                {
                    return { previous.xpos, previous.ypos };
                }
                --index;
            }
            return state.start;
        }

        /// Last positioned fragment of the batch, used to hold the final
        /// position once every fragment has been consumed.
        inline const Movement* last_positioned(const State& state)
        {
            for (size_t i = state.fragments.size(); i > 0; --i)
            {
                if (positioned(state.fragments[i - 1]))
                {
                    return &state.fragments[i - 1];
                }
            }
            return nullptr;
        }

        /// Consume 'dt' milliseconds of fragment time and return this
        /// frame's sample. Fragment positions are interpolated from the
        /// segment origin to the fragment endpoint across its duration.
        /// When the batch is exhausted the final endpoint is held: holding
        /// is deterministic on every client, while local extrapolation
        /// drifts and rubber-bands on the next packet.
        inline Sample step(State& state, double dt)
        {
            if (!state.started())
            {
                return { state.start, 0, 0, true };
            }

            double remaining = dt;
            while (state.index < state.fragments.size() && remaining > 0.0)
            {
                const Movement& fragment = state.fragments[state.index];
                const double length = positioned(fragment)
                    ? std::max<double>(fragment.duration, 0.0)
                    : 0.0;
                const double left = length - state.elapsed;
                if (remaining < left)
                {
                    state.elapsed += remaining;
                    remaining = 0.0;
                }
                else
                {
                    remaining -= left;
                    state.elapsed = 0.0;
                    ++state.index;
                }
            }

            while (state.index < state.fragments.size()
                && !positioned(state.fragments[state.index]))
            {
                ++state.index;
            }

            if (state.index >= state.fragments.size())
            {
                if (const Movement* last = last_positioned(state))
                {
                    return { { last->xpos, last->ypos }, last->fh, last->newstate, true };
                }
                return { state.start, 0, 0, true };
            }

            const Movement& fragment = state.fragments[state.index];
            const double length = std::max<double>(fragment.duration, 0.0);
            const float alpha = length > 0.0
                ? static_cast<float>(std::clamp(state.elapsed / length, 0.0, 1.0))
                : 1.0f;
            const Point<int16_t> from = origin_of(state, state.index);
            return {
                {
                    lerp(from.x(), fragment.xpos, alpha),
                    lerp(from.y(), fragment.ypos, alpha)
                },
                fragment.fh,
                fragment.newstate,
                false
            };
        }
    }
}
