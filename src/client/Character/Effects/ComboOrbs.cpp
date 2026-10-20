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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "ComboOrbs.h"

#include "../Buff.h"
#include "../Char.h"

#include "../../Gameplay/Combat/ComboAttack.h"
#include "../../Graphics/DrawArgument.h"
#include "../../Graphics/Texture.h"

#include "nlnx/nx.hpp"
#include "nlnx/node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    namespace
    {
        constexpr int32_t COMBO_ATTACK   = 1111002;
        constexpr int32_t ADVANCED_COMBO = 1120003;
        constexpr int32_t ORB_SOURCES    = 5;

        // A single pulsing orb frame from the skill's 'state' node.
        struct OrbFrame
        {
            Texture texture;
            uint16_t delay = 100;
            int16_t a0 = 255;
            int16_t a1 = 255;
        };

        // The aura and orb frames of one combo skill, loaded once.
        struct ComboVisuals
        {
            Texture aura;
            std::vector<OrbFrame> orbs;
            bool valid = false;
        };

        // Milliseconds since the steady-clock epoch.
        int64_t now_ms()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }

        // Return an integer child, or the fallback if it is absent.
        int16_t read_int(const nl::node& src, const char* name, int16_t fallback)
        {
            nl::node value = src[name];
            if (value.data_type() == nl::node::type::integer)
                return static_cast<int16_t>(value.get_integer());
            return fallback;
        }

        // Read the 'state' node of a combo skill into draw data.
        ComboVisuals load_visuals(int32_t skillid)
        {
            std::string strid = std::to_string(skillid);
            while (strid.size() < 7)
                strid.insert(strid.begin(), '0');

            nl::node state = nl::nx::skill[strid.substr(0, 3) + ".img"]
                ["skill"][strid]["state"];

            ComboVisuals visuals;
            visuals.aura = state["0"];
            for (int32_t i = 1; i <= ORB_SOURCES; ++i)
            {
                std::string name = std::to_string(i);
                nl::node frame_node = state[name];
                if (!frame_node)
                    continue;

                OrbFrame frame;
                frame.texture = frame_node;
                int16_t delay = read_int(frame_node, "delay", 100);
                frame.delay = delay > 0
                    ? static_cast<uint16_t>(delay)
                    : 100;
                frame.a0 = read_int(frame_node, "a0", 255);
                frame.a1 = read_int(frame_node, "a1", 255);
                visuals.orbs.push_back(frame);
            }
            visuals.valid = visuals.aura.is_valid() && !visuals.orbs.empty();
            return visuals;
        }

        // Return the visuals of the normal or the advanced combo skill.
        const ComboVisuals& load(bool advanced)
        {
            static const ComboVisuals combo =
                load_visuals(COMBO_ATTACK);
            static const ComboVisuals advanced_visuals =
                load_visuals(ADVANCED_COMBO);
            return advanced ? advanced_visuals : combo;
        }
    }

    void combo_orbs::draw_aura(const Char& chr, Point<int16_t> absp)
    {
        const int16_t value = chr.get_visual_buff_value(Buffstat::COMBO);
        if (value <= 0)
            return;

        // The buff packet always names the base combo skill as its source,
        // so the advanced look is derived from the skill book and the
        // counter instead of from the source id.
        const ComboVisuals& visuals =
            load(combo_attack::advanced_orbs(chr, value));
        if (!visuals.aura.is_valid())
            return;

        visuals.aura.draw(DrawArgument(absp + Point<int16_t>(0, -14)));
    }

    void combo_orbs::draw_orbs(const Char& chr, Point<int16_t> absp)
    {
        int16_t value = chr.get_visual_buff_value(Buffstat::COMBO);
        if (value <= 0)
            return;

        // Advanced Combo's extra orb from its `prop` roll is deliberately not
        // predicted here: the server rolls it and answers every hit with
        // GIVE_BUFF(COMBO) carrying the resulting count, which is used both
        // for the appearance and the number of drawn orbs.
        const ComboVisuals& visuals = load(
            combo_attack::advanced_orbs(chr, value));
        if (visuals.orbs.empty())
            return;

        // The first combo is the unactivated orb in the character's hands.
        const int16_t n = std::clamp(
            static_cast<int16_t>(value - 1),
            static_cast<int16_t>(0),
            static_cast<int16_t>(10)
        );
        if (n == 0)
            return;

        // Walk the frames to find the current one and its progress.
        int64_t total = 0;
        for (const OrbFrame& frame : visuals.orbs)
            total += frame.delay;
        if (total <= 0)
            return;

        const int64_t t = now_ms() % total;
        std::size_t index = 0;
        int64_t elapsed = 0;
        for (; index < visuals.orbs.size(); ++index)
        {
            elapsed += visuals.orbs[index].delay;
            if (t < elapsed)
                break;
        }
        if (index >= visuals.orbs.size())
            index = visuals.orbs.size() - 1;

        const OrbFrame& frame = visuals.orbs[index];
        const int64_t frame_start = elapsed - frame.delay;
        const float progress = frame.delay > 0
            ? static_cast<float>(t - frame_start) / frame.delay
            : 0.0f;
        const float pulse = frame.a0 + (frame.a1 - frame.a0) * progress;
        const float opacity = std::clamp(pulse, 0.0f, 255.0f) / 255.0f;

        // The orbs travel clockwise on an ellipse around the torso. The
        // angle offset comes from the steady clock, so the ring really orbits
        // instead of sitting on fixed slots (fixed angles made it look like a
        // static halo).
        const Point<int16_t> center = absp + Point<int16_t>(0, -42);
        constexpr float RX = 34.0f;
        constexpr float RY = 11.0f;
        constexpr float TAU = 6.28318531f;
        // One full revolution every 2.4 seconds.
        constexpr int64_t ORBIT_PERIOD_MS = 2400;
        const float phase = TAU
            * static_cast<float>(now_ms() % ORBIT_PERIOD_MS)
            / static_cast<float>(ORBIT_PERIOD_MS);

        // Draw the far half of the orbit first and dim it, so that the near
        // half overlaps the far orbs and the ring reads as three dimensional.
        for (int16_t pass = 0; pass < 2; ++pass)
        {
            for (int16_t i = 0; i < n; ++i)
            {
                const float angle = phase - TAU / 2.0f + TAU * i / n;
                const bool near_side = std::sin(angle) >= 0.0f;
                if (near_side != (pass == 1))
                    continue;

                const Point<int16_t> pos{
                    static_cast<int16_t>(std::lround(
                        center.x() + RX * std::cos(angle))),
                    static_cast<int16_t>(std::lround(
                        center.y() + RY * std::sin(angle)))
                };
                const float shade = near_side ? 1.0f : 0.65f;
                frame.texture.draw(DrawArgument(pos, opacity * shade));
            }
        }
    }
}
