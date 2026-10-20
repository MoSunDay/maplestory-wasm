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

// Audits the 079MAX4-2023 Skill.nx data contracts behind the attack-skill
// gap fixes:
//  - 3221007 (Snipe) carries no per-level damage source (no damage/mad/
//    fixdamage/x on any level), so the client falls back to a basic hit
//    while the server replaces the value with fixed 195000-199999 damage.
//  - 4211006 (Meso Explosion) likewise has no damage nodes; every level has
//    attackCount/mobCount integers with mobCount maxing at 10 (fits the
//    4-bit packet nibble) and attackCount maxing at 20 (exceeds the 15
//    nibble limit, so the client clamps to 15).
//  - 2321001 (Bishop Big Bang) exists with a magic damage ("mad") on the
//    max level as a charge-skill list member, while 2121001/2221001 exist
//    in neither this dataset nor the server wz and must stay absent.
//  - The charge list members (5201002, 5101004) and the ranged-extra list
//    members (3121004, 3221001, 5221004) exist with usable level data.
//  - 0001002 (Nimble Feet) keeps exactly three levels with mpCon 4/7/10;
//    buff duration comes from the server GIVE_BUFF, never from this node.
//  - The SkillData weapon-requirement guard is exercised on both sides:
//    every audited skill except the ranged-extra trio (3121004/3221001/
//    5221004) carries no "weapon" node, so the existence guard falls back
//    to Weapon::NONE, while the trio exposes integer weapon leaves
//    (100 + value = 145/146/149 bow/crossbow/gun family codes).
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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

    bool is_int(nl::node n)
    {
        return n.data_type() == nl::node::type::integer;
    }

    nl::node find_skill(nl::node root, const char* id)
    {
        return root[std::string(id, 3) + ".img"]["skill"][id];
    }

    bool has_any(nl::node lv, const char* const* names, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            if (present(lv[names[i]]))
                return true;
        return false;
    }

    void check_snipe(nl::node root)
    {
        const char* damage_names[] = {"damage", "mad", "fixdamage", "x"};

        nl::node node = find_skill(root, "3221007");
        check(present(node), "3221007 skill node exists");

        nl::node level = node["level"];
        check(present(level), "3221007 has a level node");
        check(level.size() == 30, "3221007 level has exactly 30 children");

        bool clean = true;
        for (auto lv : level)
            clean = clean
                && !has_any(lv, damage_names, 4);
        check(clean, "3221007 no level carries damage/mad/fixdamage/x");

        nl::node lv30 = level["30"];
        check(present(lv30), "3221007 level 30 exists");
        check(is_int(lv30["mpCon"]),
            "3221007 level 30 mpCon is an integer");
    }

    void check_meso_explosion(nl::node root)
    {
        const char* damage_names[] = {"damage", "mad", "fixdamage"};

        nl::node node = find_skill(root, "4211006");
        check(present(node), "4211006 skill node exists");

        nl::node level = node["level"];
        check(present(level), "4211006 has a level node");
        check(level.size() == 30, "4211006 level has exactly 30 children");

        bool clean = true;
        bool counters = true;
        int64_t max_mob = 0;
        int64_t max_attack = 0;
        for (auto lv : level)
        {
            clean = clean && !has_any(lv, damage_names, 3);
            counters = counters && is_int(lv["attackCount"])
                && is_int(lv["mobCount"]);
            const int64_t mob = lv["mobCount"].get_integer();
            const int64_t attack = lv["attackCount"].get_integer();
            max_mob = mob > max_mob ? mob : max_mob;
            max_attack = attack > max_attack ? attack : max_attack;
        }
        check(clean, "4211006 no level carries damage/mad/fixdamage");
        check(counters,
            "4211006 every level has attackCount/mobCount integers");
        check(max_mob == 10,
            "4211006 max mobCount is 10 (fits the 4-bit nibble)");
        check(max_attack == 20,
            "4211006 max attackCount is 20 (client clamps to 15)");
    }

    void check_big_bang(nl::node root)
    {
        nl::node bishop = find_skill(root, "2321001");
        check(present(bishop), "2321001 Bishop Big Bang exists");

        nl::node lv30 = bishop["level"]["30"];
        check(present(lv30), "2321001 level 30 exists");
        check(is_int(lv30["mad"]) && lv30["mad"].get_integer() > 0,
            "2321001 level 30 mad > 0 (charge-skill magic damage)");

        check(!present(find_skill(root, "2121001")),
            "2121001 FP Big Bang absent from dataset");
        check(!present(find_skill(root, "2221001")),
            "2221001 IL Big Bang absent from dataset");
    }

    void check_packet_layout_skills(nl::node root)
    {
        std::cout << "charge list members (server reads a 4-byte charge int)\n";
        const char* charge_ids[] = {"5201002", "5101004"};
        for (const char* id : charge_ids)
            check(present(find_skill(root, id)["level"]["1"]),
                std::string(id) + " has level 1 data");

        std::cout << "ranged-extra members (4 extra bytes in ranged header)\n";
        const char* ranged_ids[] = {"3121004", "3221001", "5221004"};
        for (const char* id : ranged_ids)
            check(present(find_skill(root, id)["level"]["1"]),
                std::string(id) + " has level 1 data");
    }

    void check_misc(nl::node root)
    {
        std::cout << "misc contracts (Nimble Feet, weapon guard)\n";
        nl::node nimble = find_skill(root, "0001002");
        check(present(nimble), "0001002 Nimble Feet exists");

        nl::node level = nimble["level"];
        check(level.size() == 3,
            "0001002 has exactly 3 levels");
        check(is_int(level["1"]["mpCon"])
                && level["1"]["mpCon"].get_integer() == 4
                && is_int(level["2"]["mpCon"])
                && level["2"]["mpCon"].get_integer() == 7
                && is_int(level["3"]["mpCon"])
                && level["3"]["mpCon"].get_integer() == 10,
            "0001002 mpCon is 4/7/10 (duration comes from server GIVE_BUFF)");

        const char* guarded[] = {
            "3221007", "4211006", "2321001", "5201002", "5101004",
            "0001002"
        };
        bool no_weapon = true;
        for (const char* id : guarded)
            no_weapon = no_weapon
                && !present(find_skill(root, id)["weapon"]);
        check(no_weapon,
            "guard-fallback skills carry no weapon node (Weapon::NONE)");

        const char* ranged_weapons[] = {"3121004", "3221001", "5221004"};
        bool weapon_leaves = true;
        for (const char* id : ranged_weapons)
        {
            nl::node weapon = find_skill(root, id)["weapon"];
            weapon_leaves = weapon_leaves && is_int(weapon)
                && weapon.size() == 0 && weapon.get_integer() > 0;
        }
        check(weapon_leaves,
            "ranged trio weapon nodes are integer leaves (guard reads them)");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: attack-gap-skills-nx Skill.nx\n";
        return 2;
    }
    nl::file file(argv[1]);
    nl::node skill = file.root();

    std::cout << "Snipe basic-hit fallback\n";
    check_snipe(skill);

    std::cout << "Meso Explosion counter clamping\n";
    check_meso_explosion(skill);

    std::cout << "Big Bang family\n";
    check_big_bang(skill);

    check_packet_layout_skills(skill);
    check_misc(skill);

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
