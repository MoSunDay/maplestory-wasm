// Audits the 079MAX4-2023 Skill.nx contracts behind the second-job attack
// whitelist in SkillData.cpp. Every whitelisted id must exist in Skill.nx:
// this tool resolves each `SkillId::NAME` in the `SkillData::flags_of` table
// through SkillId.h and asserts the id has a node in the NX book. The
// whitelisted attacks (warrior 3rd-job crashes, mage/archer/bandit/pirate
// 2nd-job attacks) must carry usable level stats and hit/effect animations,
// Arrow Bomb must carry its damage percent in "x" as fallback on every level,
// and the 111.img Panic/Coma family (1111002..1111008) must stay present with
// Armor Crash (1111007) staying damage-less.
//
// usage: second-job-attack-nx Skill.nx [SkillData.cpp] [SkillId.h]
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
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

    std::string id_string(int32_t id)
    {
        std::string text = std::to_string(id);
        if (text.size() < 7)
            text.insert(0, 7 - text.size(), '0');
        return text;
    }

    bool read_text(const std::string& path, std::string& out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;
        std::ostringstream buffer;
        buffer << in.rdbuf();
        out = buffer.str();
        return true;
    }

    // `NAME = <digits>,` entries of SkillId.h's enum.
    std::map<std::string, int32_t> parse_enum_ids(const std::string& header)
    {
        static const std::regex entry("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*([0-9]+)\\s*,");
        std::map<std::string, int32_t> ids;
        for (std::sregex_iterator it(header.begin(), header.end(), entry), end;
             it != end; ++it)
        {
            ids.emplace((*it)[1].str(), std::stoi((*it)[2].str()));
        }
        return ids;
    }

    // Braces of the `skill_flags` initializer list, found by brace matching
    // so only table rows are scanned (not the rest of flags_of).
    std::string flags_table_body(const std::string& source)
    {
        const size_t fn = source.find("SkillData::flags_of");
        if (fn == std::string::npos)
            return {};
        const size_t table = source.find("skill_flags", fn);
        if (table == std::string::npos)
            return {};
        const size_t assign = source.find('=', table);
        if (assign == std::string::npos)
            return {};
        const size_t open = source.find('{', assign);
        if (open == std::string::npos)
            return {};
        int depth = 0;
        for (size_t i = open; i < source.size(); ++i)
        {
            if (source[i] == '{')
                ++depth;
            else if (source[i] == '}' && --depth == 0)
                return source.substr(open, i - open + 1);
        }
        return {};
    }

    // `{ SkillId::NAME, <flags> }` rows of the whitelist table.
    std::vector<std::string> parse_whitelist_names(const std::string& body)
    {
        static const std::regex entry("\\{\\s*SkillId::([A-Za-z_][A-Za-z0-9_]*)\\s*,");
        std::vector<std::string> names;
        for (std::sregex_iterator it(body.begin(), body.end(), entry), end;
             it != end; ++it)
        {
            names.push_back((*it)[1].str());
        }
        return names;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: second-job-attack-nx Skill.nx [SkillData.cpp] [SkillId.h]\n";
        return 2;
    }
    const char* flags_path = argc > 2 ? argv[2] : "src/client/Data/SkillData.cpp";
    const char* ids_path = argc > 3 ? argv[3] : "src/client/Character/SkillId.h";

    nl::file file(argv[1]);
    nl::node skill = file.root();

    std::string flags_source;
    std::string ids_header;
    if (!read_text(flags_path, flags_source))
    {
        std::cerr << "cannot read whitelist source: " << flags_path << '\n';
        return 2;
    }
    if (!read_text(ids_path, ids_header))
    {
        std::cerr << "cannot read skill id header: " << ids_path << '\n';
        return 2;
    }

    std::cout << "[whitelisted ids all exist in Skill.nx]\n";
    const std::map<std::string, int32_t> enum_ids = parse_enum_ids(ids_header);
    check(!enum_ids.empty(), std::string(ids_path) + " declares SkillId constants");
    const std::string table = flags_table_body(flags_source);
    check(!table.empty(), std::string(flags_path) + " has a SkillData::flags_of table");
    const std::vector<std::string> names = parse_whitelist_names(table);
    check(!names.empty(), std::string(flags_path) + " whitelist table is non-empty");
    std::set<int32_t> whitelist;
    std::map<int32_t, std::string> whitelist_label;
    for (const auto& name : names)
    {
        const auto iter = enum_ids.find(name);
        if (iter == enum_ids.end())
        {
            check(false, "SkillId::" + name + " resolves in " + ids_path);
            continue;
        }
        whitelist.insert(iter->second);
        whitelist_label.emplace(iter->second, name);
    }
    for (int32_t id : whitelist)
    {
        const std::string sid = id_string(id);
        check(present(find_skill(skill, sid.c_str())),
              sid + " (SkillId::" + whitelist_label[id] + ") has a Skill.nx node");
    }

    // 1111007 (Armor Crash) is a damage-less debuff: every hit is 1 damage.
    const char* attack_ids[] = {
        "2201004", "2201005", "2301005", "3101005", "3201005",
        "4201004", "4201005", "5101002", "5101003", "5101004",
        "5201001", "5201002", "5201004", "5201006", "1111008",
        "1211009", "1311007", "1111003", "1111004", "1111005",
        "1111006"
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

    std::cout << "[111.img Panic/Coma family is present]\n";
    nl::node crusader = skill["111.img"]["skill"];
    const char* crusader_ids[] = {
        "1111002", "1111003", "1111004", "1111005", "1111006",
        "1111007", "1111008"
    };
    for (auto id : crusader_ids)
        check(present(crusader[id]), std::string("111.img/skill has ") + id);
    check(!present(crusader["1111007"]["level"]["1"]["damage"]),
          "1111007 level/1 stays damage-less");
    check(!present(crusader["1111007"]["level"]["1"]["x"]),
          "1111007 level/1 stays without x");
    std::vector<std::string> third_job;
    for (auto child : crusader)
        if (child.name().compare(0, 4, "1111") == 0)
            third_job.push_back(child.name());
    std::sort(third_job.begin(), third_job.end());
    const std::vector<std::string> expected_third_job = {
        "1111002", "1111003", "1111004", "1111005", "1111006",
        "1111007", "1111008"
    };
    check(third_job == expected_third_job,
          "111.img/skill 3rd-job set is exactly 1111002..1111008");

    std::cout << "[legacy ids stay absent from Skill.nx and the whitelist]\n";
    const char* legacy_ids[] = {
        "1001004", "1221011", "1311002", "1311005", "1311006",
        "2001004", "2101003", "2111004", "4001002"
    };
    for (auto id : legacy_ids)
    {
        check(!present(find_skill(skill, id)),
              std::string(id) + " has no Skill.nx node");
        check(whitelist.count(std::stoi(id)) == 0,
              std::string(id) + " is absent from the flags_of whitelist");
    }

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
