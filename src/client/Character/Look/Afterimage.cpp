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
#include "Afterimage.h"

#include "Afterimage/Resolver.h"

#include "../../Console.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

namespace jrc
{
    namespace
    {
        std::optional<int16_t> numeric_name(const std::string& name)
        {
            int16_t value = 0;
            auto result = std::from_chars(
                name.data(),
                name.data() + name.size(),
                value
            );
            if (result.ec != std::errc{} || result.ptr != name.data() + name.size())
            {
                return std::nullopt;
            }
            return value;
        }

        std::optional<uint8_t> trigger_frame(nl::node source)
        {
            std::optional<int16_t> frame = numeric_name(source.name());
            if (!frame || *frame < 0 ||
                *frame >= std::numeric_limits<uint8_t>::max())
            {
                return std::nullopt;
            }
            return static_cast<uint8_t>(*frame);
        }

        bool has_animation(nl::node stance)
        {
            for (nl::node child : stance)
            {
                if (trigger_frame(child) &&
                    child[0].data_type() == nl::node::type::bitmap)
                {
                    return true;
                }
            }
            return false;
        }

        nl::node standard_source(
            const std::string& name,
            const std::string& stance_name,
            int16_t level
        )
        {
            nl::node tiers = nl::nx::character["Afterimage"][name + ".img"];
            std::vector<int16_t> available;
            for (nl::node tier : tiers)
            {
                std::optional<int16_t> bucket = numeric_name(tier.name());
                if (bucket && *bucket >= 0 && has_animation(tier[stance_name]))
                {
                    available.push_back(*bucket);
                }
            }

            std::sort(available.begin(), available.end());
            available.erase(
                std::unique(available.begin(), available.end()),
                available.end()
            );

            int16_t requested = std::max<int16_t>(0, level / 10);
            std::optional<int16_t> selected =
                afterimage::select_level_bucket(requested, available);
            return selected ? tiers[*selected][stance_name] : nl::node{};
        }
    }

    Afterimage::Afterimage(int32_t skill_id, const std::string& name,
        const std::string& stance_name, int16_t level,
        Preparation preparation) {

        nl::node src;
        if (skill_id > 0)
        {
            std::string strid = string_format::extend_id(skill_id, 7);
            nl::node skill_src = nl::nx::skill[strid.substr(0, 3) + ".img"]
                ["skill"][strid]["afterimage"][name][stance_name];
            if (has_animation(skill_src))
            {
                src = skill_src;
            }
            else if (skill_src)
            {
                Console::get().print(
                    "Invalid skill afterimage: " + strid + "/" + name + "/" + stance_name
                );
            }
        }

        if (!has_animation(src))
        {
            src = standard_source(name, stance_name, level);
        }

        range = src;
        firstframe = 0;

        for (nl::node sub : src)
        {
            std::optional<uint8_t> frame = trigger_frame(sub);
            if (frame && sub[0].data_type() == nl::node::type::bitmap)
            {
                Animation animation(sub);
                if (animation.is_valid())
                {
                    if (preparation == Preparation::MAP_REQUIRED)
                    {
                        animation.prepare_map_required();
                    }
                    else
                    {
                        animation.prepare_effect();
                    }
                    cues.push_back({
                        std::move(animation),
                        *frame,
                        afterimage::PlaybackPhase::WAITING_FOR_TRIGGER
                    });
                }
            }
        }

        std::sort(cues.begin(), cues.end(), [](const Cue& left, const Cue& right) {
            return left.firstframe < right.firstframe;
        });
        if (!cues.empty())
        {
            firstframe = cues.front().firstframe;
        }
        else
        {
            Console::get().print(
                "Missing afterimage: " + name + "/" + stance_name +
                " at level " + std::to_string(level)
            );
        }
    }

    Afterimage::Afterimage()
    {
        firstframe = 0;
    }

    void Afterimage::draw(uint8_t stframe, const DrawArgument& args, float alpha) const
    {
        for (const Cue& cue : cues)
        {
            bool reached_now = stframe >= cue.firstframe;
            if (afterimage::should_draw(
                cue.phase,
                cue.animation.is_ready(),
                reached_now
            ))
            {
                cue.animation.draw_effect(args, alpha);
            }
        }
    }

    void Afterimage::update(uint8_t stframe, uint16_t timestep)
    {
        for (Cue& cue : cues)
        {
            cue.phase = afterimage::latch_trigger(
                cue.phase,
                stframe >= cue.firstframe
            );
            bool ready = cue.animation.is_ready();
            if (!ready && cue.phase != afterimage::PlaybackPhase::WAITING_FOR_TRIGGER &&
                cue.phase != afterimage::PlaybackPhase::COMPLETE)
            {
                // Atlas eviction and interrupted range loads use the same
                // recovery path without consuming a one-shot animation.
                cue.animation.prepare_effect();
            }
            afterimage::PlaybackPhase phase_before_assets = cue.phase;
            cue.phase = afterimage::begin_when_ready(
                cue.phase,
                ready
            );
            if (afterimage::should_advance(
                phase_before_assets,
                cue.phase,
                ready
            ))
            {
                cue.phase = afterimage::finish(
                    cue.phase,
                    cue.animation.update(timestep)
                );
            }
        }
    }

    uint8_t Afterimage::get_first_frame() const
    {
        return firstframe;
    }

    Rectangle<int16_t> Afterimage::get_range() const
    {
        return range;
    }
}
