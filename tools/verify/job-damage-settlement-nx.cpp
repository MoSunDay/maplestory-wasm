// Audits the damage-settlement chain (Combat::use_move -> Skill::apply_stats
// -> MapMobs::send_attack -> Mob::calculate_damage -> AttackPacket) for every
// attack skill whitelisted in SkillData::flags_of, grouped by the five classes
// and their job branches (Hero 112 ... Corsair 522 plus the lower jobs).
//
// Source-driven like second-job-attack-nx.cpp: the ATTACK/RANGED whitelist,
// the SkillId::NAME -> id map, the is_wide_attack family and the damage-less
// fallback exceptions are all parsed out of the sources. Against Skill.nx:
// per level a damage source (damage/mad/fixdamage, or the "x" fallback for
// Arrow Bomb; magic consumes mad only, never both), mobCount and
// attackCount/bulletCount within the 1..15 nibble (absent defaults to 1), and
// cooltime==15 for the 12 WIDE_* family only. The stock ultimates
// (Genesis/Blizzard/Meteor Shower) are the plain versions: no cooltime node.
// Meso Explosion keeps its v83 attackCount 16..20; the audit asserts the
// Skill.cpp nibble clamp. Armor Crash stays damage-less and BIG_BANG only
// needs mad at the max level.
// usage: job-damage-settlement-nx Skill.nx [String.nx] [SkillData.cpp] [SkillId.h] [Skill.cpp]
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
    int assertions = 0;
    int failures = 0;
    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        ++assertions;
        if (!ok)
            ++failures;
    }

    using IdMap = std::map<std::string, int32_t>;
    using NameNodes = std::vector<std::pair<int, nl::node>>;
    using Row = std::pair<std::string, std::string>;

    bool read_text(const std::string& path, std::string& out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        out = (std::ostringstream() << in.rdbuf()).str();
        return true;
    }

    std::string id_string(int32_t id)
    {
        std::string text = std::to_string(id);
        if (text.size() < 7) text.insert(0, 7 - text.size(), '0');
        return text;
    }

    nl::node find_skill(nl::node root, const std::string& sid)
    {
        return root[sid.substr(0, 3) + ".img"]["skill"][sid];
    }

    // `NAME = <digits>,` entries of the SkillId.h enum.
    IdMap parse_enum_ids(const std::string& header)
    {
        static const std::regex entry("([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*([0-9]+)\\s*,");
        IdMap ids;
        for (std::sregex_iterator it(header.begin(), header.end(), entry), end; it != end; ++it)
            ids.emplace((*it)[1].str(), std::stoi((*it)[2].str()));
        return ids;
    }

    // Braces following `anchor`, matched so nested rows stay inside.
    std::string brace_body(const std::string& source, const std::string& anchor)
    {
        const size_t open = source.find('{', source.find(anchor));
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

    // `{ SkillId::NAME, FLAGS }` rows of the whitelist table.
    std::vector<Row> parse_flag_rows(const std::string& body)
    {
        static const std::regex row("\\{\\s*SkillId::([A-Za-z_][A-Za-z0-9_]*)\\s*,\\s*([^}]+?)\\s*\\}");
        std::vector<Row> rows;
        for (std::sregex_iterator it(body.begin(), body.end(), row), end; it != end; ++it)
            rows.emplace_back((*it)[1].str(), (*it)[2].str());
        return rows;
    }

    // `var == SkillId::NAME` comparisons, e.g. the damage fallback exceptions.
    std::set<std::string> parse_id_conditions(const std::string& source, const std::string& var)
    {
        const std::regex cond(var + "\\s*==\\s*SkillId::([A-Za-z_][A-Za-z0-9_]*)");
        std::set<std::string> names;
        for (std::sregex_iterator it(source.begin(), source.end(), cond), end; it != end; ++it)
            names.insert((*it)[1].str());
        return names;
    }

    // `id == NAME` chain inside SkillId::is_wide_attack.
    std::vector<std::string> parse_wide_names(const std::string& header)
    {
        static const std::regex cmp("==\\s*([A-Za-z_][A-Za-z0-9_]*)");
        std::vector<std::string> names;
        const std::string body = brace_body(header, "is_wide_attack");
        for (std::sregex_iterator it(body.begin(), body.end(), cmp), end; it != end; ++it)
            names.push_back((*it)[1].str());
        return names;
    }

    // Positive-integer level children of the skill node.
    NameNodes level_nodes(nl::node node)
    {
        NameNodes levels;
        for (nl::node level : node["level"])
        {
            const std::string& name = level.name();
            if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos)
                if (int no = std::stoi(name); no > 0) levels.emplace_back(no, level);
        }
        return levels;
    }

    std::vector<std::string> split_tokens(const std::string& expr)
    {
        std::vector<std::string> tokens;
        std::istringstream in(expr);
        std::string token;
        while (std::getline(in, token, '|'))
        {
            const size_t a = token.find_first_not_of(" \t"), b = token.find_last_not_of(" \t");
            if (a != std::string::npos) tokens.push_back(token.substr(a, b - a + 1));
        }
        return tokens;
    }

    bool has_token(const std::string& expr, const char* want)
    {
        const std::vector<std::string> tokens = split_tokens(expr);
        return std::find(tokens.begin(), tokens.end(), std::string(want)) != tokens.end();
    }

    std::string job_name(int32_t job)
    {
        static const std::map<int32_t, std::string> names = {
            { 0, "Beginner" }, { 100, "Swordman" }, { 110, "Fighter" }, { 111, "Crusader" }, { 112, "Hero" },
            { 120, "Page" }, { 121, "White Knight" }, { 122, "Paladin" }, { 130, "Spearman" }, { 131, "Dragon Knight" },
            { 132, "Dark Knight" }, { 200, "Magician" }, { 210, "F/P Wizard" }, { 211, "F/P Mage" }, { 212, "F/P ArchMage" },
            { 220, "I/L Wizard" }, { 221, "I/L Mage" }, { 222, "I/L ArchMage" }, { 230, "Cleric" }, { 231, "Priest" },
            { 232, "Bishop" }, { 300, "Archer" }, { 310, "Hunter" }, { 311, "Ranger" }, { 312, "Bowmaster" },
            { 320, "Crossbowman" }, { 321, "Sniper" }, { 322, "Marksman" }, { 400, "Rogue" }, { 410, "Assassin" },
            { 411, "Hermit" }, { 412, "Night Lord" }, { 420, "Bandit" }, { 421, "Chief Bandit" }, { 422, "Shadower" },
            { 500, "Pirate" }, { 510, "Brawler" }, { 511, "Marauder" }, { 512, "Buccaneer" }, { 520, "Gunslinger" },
            { 521, "Outlaw" }, { 522, "Corsair" } };
        const auto it = names.find(job);
        return it != names.end() ? it->second : "job " + std::to_string(job);
    }

    std::string class_name(int32_t cls)
    {
        static const std::map<int32_t, std::string> names = {
            { 0, "新手 Beginner" }, { 1, "战士 Warrior" }, { 2, "法师 Magician" },
            { 3, "弓手 Bowman" }, { 4, "飞侠 Thief" }, { 5, "海盗 Pirate" } };
        const auto it = names.find(cls);
        return it != names.end() ? it->second : "class " + std::to_string(cls);
    }

    // Nibble bound: absent counts as the get_integer(1) default of SkillData.
    // cap is 15, or 20 for the v83 Meso Explosion attackCount exception.
    void tally_count(nl::node level, const char* key, long cap, long& maxv, int& bad)
    {
        const long value = level[key].get_integer(-1);
        if (value >= 0)
        {
            maxv = std::max(maxv, value);
            if (value < 1 || value > cap) ++bad;
        }
    }

    // source_kind: 0 = every level needs a damage source, 1 = damage-less allowed
    // (SkillData fallback / Armor Crash contract), 2 = BIG_BANG (mad at max level).
    // wide = WIDE_* 15s cooldown contract, plain = stock ultimate without one,
    // v83_hits = Meso Explosion attackCount 16..20, clamped at clamp_ref.
    void audit_skill(nl::node skill_root, nl::node strings, int32_t id, const std::string& label,
                     bool ranged, int source_kind, bool wide, bool plain, bool v83_hits,
                     const std::string& clamp_ref)
    {
        const std::string sid = id_string(id);
        nl::node node = find_skill(skill_root, sid);
        const NameNodes levels = node ? level_nodes(node) : NameNodes();
        check(static_cast<bool>(node) && !levels.empty(), sid + " (" + label + ") has <book>.img/skill node with level children");
        int sourced = 0, both = 0, mob_bad = 0, atk_bad = 0, bul_bad = 0, cd_hot = 0;
        long mob_max = 1, atk_max = 1, bul_max = 1, cd_bad = 0;
        long max_level = 0, max_d = 0, max_m = 0, max_f = 0, max_x = 0;
        std::set<long> cds;
        for (const auto& entry : levels)
        {
            nl::node level = entry.second;
            const long d = level["damage"], m = level["mad"], f = level["fixdamage"], x = level["x"];
            if (d > 0 || m > 0 || f > 0 || x > 0) ++sourced;
            if (d > 0 && m > 0) ++both;
            if (entry.first >= max_level) { max_level = entry.first; max_d = d; max_m = m; max_f = f; max_x = x; }
            tally_count(level, "mobCount", 15, mob_max, mob_bad);
            tally_count(level, "attackCount", v83_hits ? 20 : 15, atk_max, atk_bad);
            tally_count(level, "bulletCount", 15, bul_max, bul_bad);
            const long cd = level["cooltime"].get_integer(-1);
            if (cd >= 0) { cds.insert(cd); if (cd != 15) ++cd_bad; if (cd > 0) ++cd_hot; }
        }
        const std::string name = strings["Skill.img"][sid]["name"].get_string("?");
        const long lines = ranged ? bul_max : atk_max;
        const char* source = max_d > 0 ? "damage" : max_m > 0 ? "mad" : max_f > 0 ? "fixdamage" : max_x > 0 ? "damage(x)" : "无伤害源";
        const long value = max_d > 0 ? max_d : max_m > 0 ? max_m : max_f > 0 ? max_f : max_x;
        const std::string cd_text = cds.size() == 1 ? std::to_string(*cds.begin()) : "none";
        if (wide)
            std::cout << "  " << name << "(" << sid << "): " << levels.size() << " levels, " << source << " " << value << "x" << lines << ", mobs " << mob_max << ", cd " << cd_text << '\n';
        else
            std::cout << "  " << sid << " " << name << ": " << levels.size() << " levels, " << source << (value > 0 ? " " + std::to_string(value) + "x" + std::to_string(lines) : "") << '\n';
        if (source_kind == 0)
            check(sourced == static_cast<int>(levels.size()), sid + " carries a damage source on every level (damage/mad/fixdamage/x: " + std::to_string(sourced) + "/" + std::to_string(levels.size()) + ")");
        else if (source_kind == 1)
            check(true, sid + " " + label + " damage-less allowed (SkillData fallback / Armor Crash contract)");
        else
            check(max_m > 0, sid + " BIG_BANG max level " + std::to_string(max_level) + " carries mad>0 (lower levels optional)");
        check(both == 0, sid + " keeps damage and mad mutually exclusive (magic consumes mad only)" + (both ? " - " + std::to_string(both) + " levels carry both" : ""));
        check(mob_bad == 0, sid + " mobCount within 1..15 (max " + std::to_string(mob_max) + ", absent defaults to 1)");
        if (v83_hits)
            check(atk_bad == 0 && atk_max > 15 && !clamp_ref.empty(),
                  sid + " attackCount 16..20 (v83 data) - client clamps to 15 (" + clamp_ref + ")");
        else
            check(atk_bad == 0, sid + " attackCount within 1..15 (max " + std::to_string(atk_max) + ", absent defaults to 1)" + (atk_bad ? " - " + std::to_string(atk_bad) + " levels out of range" : ""));
        if (ranged)
            check(bul_bad == 0, sid + " RANGED bulletCount within 1..15 (hitcount source, max " + std::to_string(bul_max) + ", absent defaults to 1)");
        if (wide)
            check(!levels.empty() && cd_bad == 0 && cds.size() == 1 && *cds.begin() == 15,
                  sid + " cooltime==15 on all " + std::to_string(levels.size()) + " levels");
        if (plain)
            check(cd_hot == 0 && !levels.empty(), sid + " " + label + ": 普通版本无冷却 (" + std::to_string(levels.size())
                  + " levels, " + source + " " + std::to_string(value) + "x" + std::to_string(lines) + ")");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: job-damage-settlement-nx Skill.nx [String.nx] [SkillData.cpp] [SkillId.h] [Skill.cpp]\n"; return 2; }
    // Extra args are routed by extension: Skill.nx String.nx / SkillData.cpp SkillId.h both work.
    std::string string_path = "assets/String.nx", flags_path = "src/client/Data/SkillData.cpp";
    std::string ids_path = "src/client/Character/SkillId.h", combat_path = "src/client/Gameplay/Combat/Skill.cpp";
    std::vector<std::string> extra_cpp, extra_h, extra_nx;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg.size() > 3 && arg.compare(arg.size() - 3, 3, ".nx") == 0) extra_nx.push_back(arg);
        else if (arg.size() > 4 && arg.compare(arg.size() - 4, 4, ".cpp") == 0) extra_cpp.push_back(arg);
        else if (arg.size() > 2 && arg.compare(arg.size() - 2, 2, ".h") == 0) extra_h.push_back(arg);
    }
    if (!extra_nx.empty()) string_path = extra_nx.front();
    if (!extra_cpp.empty()) { flags_path = extra_cpp.front(); if (extra_cpp.size() > 1) combat_path = extra_cpp[1]; }
    if (!extra_h.empty()) ids_path = extra_h.front();

    std::string flags_source, ids_header, combat_source;
    if (!read_text(flags_path, flags_source) || !read_text(ids_path, ids_header) || !read_text(combat_path, combat_source))
    { std::cerr << "cannot read SkillData.cpp/SkillId.h/Skill.cpp\n"; return 2; }
    nl::file skill_file(argv[1]);
    nl::file string_file(string_path);
    nl::node skill = skill_file.root();
    nl::node strings = string_file.root();

    std::cout << "[source-driven parse]\n";
    const IdMap enum_ids = parse_enum_ids(ids_header);
    check(!enum_ids.empty(), ids_path + " declares SkillId constants (" + std::to_string(enum_ids.size()) + ")");
    const std::string table = brace_body(flags_source, "skill_flags");
    check(!table.empty(), flags_path + " has the flags_of skill_flags table");
    const std::vector<Row> rows = parse_flag_rows(table);
    check(!rows.empty(), "whitelist table rows: " + std::to_string(rows.size()));
    std::set<std::string> bits;
    for (const Row& row : rows) for (const std::string& token : split_tokens(row.second)) bits.insert(token);
    std::string unknown_bits;
    for (const std::string& bit : bits)
        if (bit != "ATTACK" && bit != "RANGED" && bit != "NONE") unknown_bits += bit + " ";
    check(unknown_bits.empty(), std::string("flag bits in use: ATTACK/RANGED only (") + (unknown_bits.empty() ? "matches SkillData::Flags)" : "unknown: " + unknown_bits + ")"));

    std::map<int32_t, std::string> labels;
    std::set<int32_t> attack_ids, ranged_ids;
    for (const Row& row : rows)
    {
        const auto iter = enum_ids.find(row.first);
        if (iter == enum_ids.end()) { check(false, "SkillId::" + row.first + " resolves in " + ids_path); continue; }
        if (!has_token(row.second, "ATTACK")) continue;
        labels.emplace(iter->second, row.first);
        attack_ids.insert(iter->second);
        if (has_token(row.second, "RANGED")) ranged_ids.insert(iter->second);
    }
    check(!attack_ids.empty(), "whitelist ATTACK skills: " + std::to_string(attack_ids.size())
          + " (RANGED " + std::to_string(ranged_ids.size()) + ")");

    std::cout << "[wide family from is_wide_attack]\n";
    std::set<int32_t> wide_ids;
    for (const std::string& name : parse_wide_names(ids_header))
        if (const auto iter = enum_ids.find(name); iter != enum_ids.end())
            wide_ids.insert(iter->second);
        else
            check(false, "wide member SkillId::" + name + " resolves");
    std::set<int32_t> expected_wide;
    for (const auto& entry : enum_ids)
        if (entry.first.compare(0, 5, "WIDE_") == 0) expected_wide.insert(entry.second);
    check(wide_ids == expected_wide && !expected_wide.empty(),
          "is_wide_attack set = the 12 WIDE_* family only, stock ultimates excluded ("
          + std::to_string(wide_ids.size()) + " ids)");
    const bool wide_whitelisted = std::all_of(wide_ids.begin(), wide_ids.end(),
        [&](int32_t id) { return attack_ids.count(id) > 0; });
    check(wide_whitelisted && !wide_ids.empty(), "WIDE_* set is a subset of the ATTACK whitelist");

    std::cout << "[stock ultimates stay plain: no cooldown]\n";
    std::set<int32_t> plain_ids;
    for (const char* ultimate : { "GENESIS", "BLIZZARD", "METEOR_SHOWER" })
        if (const auto iter = enum_ids.find(ultimate); iter != enum_ids.end())
            plain_ids.insert(iter->second);
    check(plain_ids.size() == 3, "SkillId.h declares GENESIS/BLIZZARD/METEOR_SHOWER (" + std::to_string(plain_ids.size()) + "/3)");
    std::string clamp_ref;
    static const std::regex clamp_re("hitcount\\s*=\\s*std::min\\(\\s*stats\\.attackcount\\s*,\\s*uint8_t\\(15\\)\\s*\\)");
    std::smatch clamp_m;
    if (std::regex_search(combat_source, clamp_m, clamp_re))
    {
        const size_t slash = combat_path.find_last_of('/');
        clamp_ref = combat_path.substr(slash + 1) + ":" + std::to_string(1 + std::count(combat_source.begin(), combat_source.begin() + clamp_m.position(), '\n'));
    }
    check(!clamp_ref.empty(), "Skill.cpp clamps non-ranged hitcount to the nibble bound (std::min(stats.attackcount, uint8_t(15)) at " + clamp_ref + ")");
    const int32_t meso_id = enum_ids.count("MESO_EXPLOSION") ? enum_ids.at("MESO_EXPLOSION") : -1;

    std::cout << "[damage-source exceptions]\n";
    std::map<int32_t, int> kinds;
    for (const std::string& name : parse_id_conditions(flags_source, "id"))
        if (const auto iter = enum_ids.find(name); iter != enum_ids.end()) kinds[iter->second] = 1;
    if (const auto armor = enum_ids.find("ARMOR_CRASH"); armor != enum_ids.end()) kinds[armor->second] = 1;
    if (const auto bang = enum_ids.find("BIG_BANG"); bang != enum_ids.end()) kinds[bang->second] = 2;
    const int32_t exempt_whitelisted = static_cast<int32_t>(std::count_if(kinds.begin(), kinds.end(),
        [&](const std::pair<const int32_t, int>& k) { return attack_ids.count(k.first) > 0; }));
    check(exempt_whitelisted == static_cast<int32_t>(kinds.size()),
          "exception skills stay inside the ATTACK whitelist (SNIPE/MESO_EXPLOSION code fallback, Armor Crash contract, BIG_BANG max-level)");
    std::cout << "[differentiated effect sources]\n";
    const std::string tint = brace_body(flags_source, "wide_attack_icon_tint");
    check(tint.find("is_wide_attack") != std::string::npos && tint.find("{ 0.78f, 0.55f, 1.0f, 1.0f }") != std::string::npos,
          "SkillData.cpp wide_attack_icon_tint gates on is_wide_attack and returns { 0.78f, 0.55f, 1.0f, 1.0f }");
    static const std::regex flourish_pair("\"[^\"]+\"\\s*,\\s*\"[^\"]+\"");
    const std::string flourish = brace_body(combat_source, "FLOURISH_EFFECTS");
    check(std::regex_search(flourish, flourish_pair), "Skill.cpp FLOURISH_EFFECTS carries a non-empty {book, skill} pair");
    check(combat_source.find("SkillId::is_wide_attack(skillid)") != std::string::npos,
          "Skill.cpp flourish block is gated by SkillId::is_wide_attack(skillid)");
    std::cout << (ranged_ids.empty() ? "  ..  flags_of carries no RANGED bit; ranged checks skipped\n"
        : "  ..  ranged hitcount comes from bulletCount (Skill.cpp apply_stats RANGED case)\n");

    std::map<int32_t, std::map<int32_t, std::vector<int32_t>>> by_job;
    for (int32_t id : attack_ids)
        by_job[id >= 1000000 ? id / 1000000 : 0][id / 10000].push_back(id);
    int32_t groups = 0;
    for (const auto& cls : by_job)
    {
        std::cout << "== " << class_name(cls.first) << " ==\n";
        for (const auto& job : cls.second)
        {
            std::cout << "[" << job_name(job.first) << " " << job.first << "]\n";
            ++groups;
            for (int32_t id : job.second)
            {
                const auto kind = kinds.find(id);
                audit_skill(skill, strings, id, labels[id], ranged_ids.count(id) > 0,
                            kind == kinds.end() ? 0 : kind->second, wide_ids.count(id) > 0,
                            plain_ids.count(id) > 0, id == meso_id, clamp_ref);
            }
        }
    }

    std::cout << "[stats] skills " << attack_ids.size() << ", groups " << groups
              << ", assertions " << assertions << ", failures " << failures << '\n';
    std::cout << (failures ? "RESULT: FAILED\n" : "RESULT: PASSED\n");
    return failures ? 1 : 0;
}
