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

// Audits the data contracts behind the special-attack effect fixes:
//  - Meso Explosion (4211006): the "effect" node is the coin burst played
//    at each detonated meso pile (REMOVE_ITEM_FROM_MAP mode 4), not a
//    caster effect; it must expose contiguous bitmap frames, and the
//    detonation sound is Sound/Skill.img/4211006/Hit. The skill carries no
//    "effect0", so nothing else renders on the caster.
//  - Flash Jump (4111006): the skill node exists with levels (hs/mpCon)
//    and a use effect; the client drives the dash itself, the dataset
//    carries no distance values.
//  - drop_removal policy: mode 4 maps to MESO_EXPLOSION with a character
//    id, and modes 0/1/2/5 keep their previous effects.
#include <cstdint>
#include <iostream>
#include <string>

#include "Gameplay/MapleMap/DropLifecycle/RemovalPolicy.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// Metadata-only audit: stub the constructor so bitmap.cpp is not linked.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
}

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        ++checks;
        if (!ok)
            ++failures;
    }

    bool present(nl::node n)
    {
        return static_cast<bool>(n);
    }

    void check_meso_explosion_effect(nl::node skill_root)
    {
        nl::node effect =
            skill_root["421.img"]["skill"]["4211006"]["effect"];
        check(present(effect), "4211006 effect node exists");

        size_t frames = 0;
        bool contiguous = true;
        for (int expected = 0; present(effect[std::to_string(expected)]); ++expected)
        {
            contiguous = contiguous
                && effect[std::to_string(expected)].data_type()
                    == nl::node::type::bitmap;
            ++frames;
        }
        check(frames >= 8 && contiguous,
            "4211006 effect exposes >=8 contiguous bitmap frames");
        check(frames == 11,
            "4211006 effect has exactly 11 frames (0..10)");

        check(!present(skill_root["421.img"]["skill"]["4211006"]["effect0"]),
            "4211006 has no effect0 (caster keeps NoUseEffect)");
    }

    void check_meso_explosion_sound(nl::node sound_root)
    {
        nl::node hit = sound_root["Skill.img"]["4211006"]["Hit"];
        check(present(hit) && hit.data_type() == nl::node::type::audio,
            "Sound/Skill.img/4211006/Hit is an audio node");
    }

    void check_flash_jump(nl::node skill_root)
    {
        nl::node node = skill_root["411.img"]["skill"]["4111006"];
        check(present(node), "4111006 skill node exists");
        check(present(node["effect0"]),
            "4111006 has a use effect (effect0)");

        nl::node level = node["level"];
        check(present(level) && level.size() == 20,
            "4111006 has 20 levels");

        bool no_distance = true;
        for (auto lv : level)
            no_distance = no_distance && !present(lv["x"]) && !present(lv["damage"]);
        check(no_distance,
            "4111006 levels carry no distance/damage (client-side dash)");
    }

    void check_removal_policy()
    {
        using jrc::drop_removal::Effect;
        using jrc::drop_removal::policy_for_mode;

        constexpr auto exploded = policy_for_mode(4);
        check(exploded.effect == Effect::MESO_EXPLOSION
                && exploded.has_character_id && !exploded.has_pet_index
                && exploded.known_mode,
            "mode 4 maps to MESO_EXPLOSION with character id");
        check(policy_for_mode(0).effect == Effect::EXPIRY_FADE
                && policy_for_mode(1).effect == Effect::IMMEDIATE_ERASE
                && policy_for_mode(2).effect == Effect::PLAYER_PICKUP
                && policy_for_mode(5).effect == Effect::PET_PICKUP,
            "modes 0/1/2/5 keep their effects");
        check(!policy_for_mode(3).known_mode
                && policy_for_mode(99).effect == Effect::IMMEDIATE_ERASE,
            "unknown modes still converge to IMMEDIATE_ERASE");
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <Skill.nx> <Sound.nx>\n";
        return 1;
    }

    nl::file skills_file(argv[1]);
    nl::file sound_file(argv[2]);

    std::cout << "Meso Explosion / Flash Jump data contract\n";
    check_meso_explosion_effect(skills_file.root());
    check_meso_explosion_sound(sound_file.root());
    check_flash_jump(skills_file.root());
    check_removal_policy();

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
