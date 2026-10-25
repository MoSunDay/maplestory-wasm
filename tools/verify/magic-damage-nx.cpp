// Audits the P1 magic-damage fix data contract: wand/staff users scale the
// base range with MAGIC (incMAD) instead of WATK, magic skills read "mad" from
// Skill.nx as a percentage (server AbstractDealDamageHandler getMatk()) while
// the "damage" percent stays unused for magic, and the 079MAX4-2023 magic
// skills keep the max-level "mad" values the client now multiplies by.
//
// Usage: magic-damage-nx assets/Skill.nx
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#include "Character/AttackStat.h"
#include "Gameplay/Combat/MagicDamage.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// Metadata-only audit: stub the constructor so bitmap.cpp is not linked.
namespace nl
{
    bitmap::bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
}

namespace
{
    int failures = 0;

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        if (!ok)
            ++failures;
    }

    bool near(double actual, double expected)
    {
        return std::fabs(actual - expected) <= 1e-9;
    }

    // Skill ids are 7 digits; the first three name the wrapper book.
    nl::node find_skill(nl::node root, const std::string& id)
    {
        return root[id.substr(0, 3) + ".img"]["skill"][id];
    }

    int max_level(nl::node skill)
    {
        int highest = 0;
        for (auto child : skill["level"])
        {
            const std::string name = child.name();
            if (name.empty())
                continue;
            const int level = std::stoi(name);
            if (level > highest)
                highest = level;
        }
        return highest;
    }

    struct magic_skill
    {
        const char* id;
        const char* name;
        int64_t mad;
    };

    // Max-level "mad" of the magic skills the fix relies on.
    const magic_skill magic_skills[] = {
        { "2101004", "Fire Arrow", 200 },
        { "2121006", "Paralyze", 370 },
        { "2121007", "Meteor Shower", 500 },
        { "2221006", "Chain Lightning", 660 },
        { "2321007", "Angel Ray", 800 },
    };
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: magic-damage-nx Skill.nx\n";
        return 2;
    }

    std::cout << "[attack stat selection]\n";
    check(jrc::attack_stat::of(jrc::Weapon::WAND) == jrc::Equipstat::MAGIC,
          "WAND scales with MAD (Equipstat::MAGIC)");
    check(jrc::attack_stat::of(jrc::Weapon::STAFF) == jrc::Equipstat::MAGIC,
          "STAFF scales with MAD (Equipstat::MAGIC)");
    check(jrc::attack_stat::of(jrc::Weapon::SWORD_1H) == jrc::Equipstat::WATK,
          "SWORD_1H still scales with WATK");
    check(jrc::attack_stat::of(jrc::Weapon::BOW) == jrc::Equipstat::WATK,
          "BOW still scales with WATK");

    std::cout << "[mad percent scaling]\n";
    check(near(jrc::magic_damage::scale(200), 2.0), "scale(200) == 2.0");
    check(near(jrc::magic_damage::scale(100), 1.0), "scale(100) == 1.0");
    check(near(jrc::magic_damage::apply(1000.0, 200), 2000.0),
          "apply(1000, 200) == 2000");

    nl::file file(argv[1]);
    nl::node skill = file.root();

    std::cout << "[max-level mad]\n";
    for (auto entry : magic_skills)
    {
        nl::node node = find_skill(skill, entry.id);
        const std::string id = entry.id;
        const int level = max_level(node);
        const int64_t mad = node["level"][std::to_string(level)]["mad"].get_integer();
        check(static_cast<bool>(node) && level == 30,
              id + " (" + entry.name + ") has 30 levels");
        check(mad == entry.mad,
              id + " max-level mad == " + std::to_string(entry.mad)
                  + " (got " + std::to_string(mad) + ")");

        const double expected = static_cast<double>(entry.mad) * 10.0;
        const double scaled = jrc::magic_damage::apply(1000.0, static_cast<int32_t>(entry.mad));
        check(near(scaled, expected),
              id + " apply(1000, " + std::to_string(entry.mad) + ") == "
                  + std::to_string(static_cast<int64_t>(expected)));
    }

    std::cout << "[wide magic ignores damage]\n";
    // 2121009 is a custom wide magic skill: by the mutual-exclusion contract
    // it carries "mad" (330) only and no "damage" node.  The server multiplies
    // magic by mad only, so the range grows by 3.3x, never by a damage percent.
    nl::node wide = find_skill(skill, "2121009");
    const nl::node level10 = wide["level"]["10"];
    const int64_t wide_damage = level10["damage"].get_integer();
    const int64_t wide_mad = level10["mad"].get_integer();
    check(max_level(wide) == 10, "2121009 has 10 levels");
    check(wide_damage == 0,
          "2121009 level/10 damage node absent (got " + std::to_string(wide_damage) + ")");
    check(wide_mad == 330,
          "2121009 level/10 mad == 330 (got " + std::to_string(wide_mad) + ")");
    check(near(jrc::magic_damage::scale(static_cast<int32_t>(wide_mad)), 3.3),
          "scale(330) == 3.3");
    check(!near(jrc::magic_damage::scale(static_cast<int32_t>(wide_mad)), 7.0),
          "scale(330) != 7.0 (magic ignores damage)");
    check(near(jrc::magic_damage::apply(1000.0, static_cast<int32_t>(wide_mad)), 3300.0),
          "apply(1000, 330) == 3300, not the 7000 damage-percent result");
    check(!near(jrc::magic_damage::apply(1000.0, static_cast<int32_t>(wide_mad)), 7000.0),
          "apply(1000, 330) != 7000");

    std::cout << (failures ? "RESULT: FAILED\n" : "RESULT: PASSED\n");
    return failures ? 1 : 0;
}
