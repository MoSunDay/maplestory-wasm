// Audits the 079MAX4-2023 Skill.nx data contracts behind the second-job
// attack whitelist fix in SkillData.cpp: the newly whitelisted attack skills
// (warrior 3rd-job crashes, mage/archer/bandit/pirate 2nd-job attacks) must
// exist with usable level stats and hit/effect animations, Arrow Bomb must
// carry its damage percent in "x" on every level, and the 079MAX4 skill-set
// regressions (removed Power Strike / Panic family) must stay stable.
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

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        if (!ok)
            ++failures;
    }

    bool present(nl::node n)
    {
        return static_cast<bool>(n);
    }

    // Skill ids are 7 digits; the first three name the wrapper book.
    nl::node find_skill(nl::node root, const char* id)
    {
        return root[std::string(id, 3) + ".img"]["skill"][id];
    }

    long positive_stat(nl::node level)
    {
        const long damage = level["damage"].get_integer();
        if (damage > 0)
            return damage;
        const long mad = level["mad"].get_integer();
        if (mad > 0)
            return mad;
        return level["x"].get_integer();
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: second-job-attack-nx Skill.nx\n";
        return 2;
    }
    nl::file file(argv[1]);
    nl::node skill = file.root();

    // 1111007 (Armor Crash) is a damage-less debuff: every hit is 1 damage.
    const char* attack_ids[] = {
        "2201004", "2201005", "2301005", "3101005", "3201005",
        "4201004", "4201005", "5101002", "5101003", "5101004",
        "5201001", "5201002", "5201004", "5201006", "1111008",
        "1211009", "1311007"
    };

    std::cout << "[new attack skills present with level 1 stats]\n";
    for (auto id : attack_ids)
    {
        nl::node node = find_skill(skill, id);
        check(present(node), std::string(id) + " exists in Skill.nx");
        if (!present(node))
            continue;
        nl::node level1 = node["level"]["1"];
        check(present(level1), std::string(id) + " has level/1");
        check(positive_stat(level1) > 0,
              std::string(id) + " level/1 has damage/mad/x > 0");
    }

    std::cout << "[armor crash is the damage-less exception]\n";
    nl::node armor_crash = find_skill(skill, "1111007");
    check(present(armor_crash), "1111007 exists in Skill.nx");
    check(present(armor_crash["level"]["1"]), "1111007 has level/1");
    check(!present(armor_crash["level"]["1"]["damage"]),
          "1111007 level/1 has no damage field");
    check(!present(armor_crash["level"]["1"]["x"]),
          "1111007 level/1 has no x field");

    std::cout << "[arrow bomb damage percent lives in x]\n";
    nl::node bomb = find_skill(skill, "3101005");
    bool every_level_x = present(bomb["level"]);
    long levels_checked = 0;
    for (auto level : bomb["level"])
    {
        ++levels_checked;
        if (level["x"].get_integer() <= 0)
            every_level_x = false;
    }
    check(levels_checked == 30, "3101005 has 30 levels");
    check(every_level_x, "3101005 every level has x > 0");
    check(bomb["level"]["1"]["x"].get_integer() == 72,
          "3101005 level/1 x == 72");
    check(bomb["level"]["30"]["x"].get_integer() == 220,
          "3101005 level/30 x == 220");

    std::cout << "[079MAX4 skill set regressions]\n";
    check(!present(skill["100.img"]["skill"]["1001004"]),
          "100.img/skill has no 1001004 (Power Strike removed)");
    nl::node crusader = skill["111.img"]["skill"];
    const char* crusader_kept[] = { "1111002", "1111005", "1111007", "1111008" };
    for (auto id : crusader_kept)
        check(present(crusader[id]),
              std::string("111.img/skill keeps ") + id);
    const char* crusader_dropped[] = { "1111003", "1111004", "1111006" };
    for (auto id : crusader_dropped)
        check(!present(crusader[id]),
              std::string("111.img/skill drops ") + id);
    std::vector<std::string> third_job;
    for (auto child : crusader)
        if (child.name().compare(0, 4, "1111") == 0)
            third_job.push_back(child.name());
    check(third_job.size() == 4
          && third_job == std::vector<std::string>(
              { crusader_kept[0], crusader_kept[1], crusader_kept[2],
                crusader_kept[3] }),
          "111.img/skill 3rd-job set is exactly 1111002/1111005/1111007/1111008");

    std::cout << "[hit or effect animation present]\n";
    for (auto id : attack_ids)
    {
        nl::node node = find_skill(skill, id);
        const bool animated = present(node["hit"]) || present(node["effect"])
            || present(node["finish"]);
        check(animated, std::string(id) + " has hit or effect/finish node");
    }
    check(present(armor_crash["hit"]) || present(armor_crash["effect"])
          || present(armor_crash["finish"]),
          "1111007 has hit or effect/finish node");

    std::cout << (failures ? "RESULT: FAILED\n" : "RESULT: PASSED\n");
    return failures ? 1 : 0;
}
