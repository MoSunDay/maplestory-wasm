// Audits the 12 wide-attack skills grafted on top of the v83 explorer
// 4th-job books (tools/nx/wide_attack_manifest.json): each target sits under
// <job>.img/skill/<id> beside its source skill, carries exactly ten usable
// levels (attackCount/mobCount/mpCon/cooltime/lt/rb/range with damage, or
// mad for the three magic skills which must not carry damage, plus
// bulletCount/bulletConsume for ranged), keeps a hit animation
// and no invisible/skillType flags, and has a matching String.nx
// Skill.img/<7-digit id> name/desc/h1..h10 block.  The source skill must keep
// its hit and action animation, and Night Lord's copy must not inherit the
// source's "alert2" stance.
//
// Usage: wide-attack-nx assets/Skill.nx assets/String.nx
#include <cstdint>
#include <exception>
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

    // Every one of the ten levels repeats the same stat block.
    struct stats
    {
        int64_t attack_count;
        int64_t damage;         // 0 (absent) for the three magic skills
        int64_t mob_count;
        int64_t mp_con;
        int64_t cooltime;
        int64_t lt_x;
        int64_t lt_y;
        int64_t rb_x;
        int64_t rb_y;
        int64_t range;
        int64_t mad;            // > 0 for the three magic skills
        int64_t bullet_count;   // > 0 for the three ranged skills
        int64_t bullet_consume;
    };

    struct target
    {
        const char* id;
        const char* img;
        const char* source_id;
        const char* source_img;
        const char* source_action; // expected source action/0, nullptr = none
        const char* name;
        const char* desc;
        const char* action;        // expected target action/0, nullptr = any
        stats levels;
    };

    const target targets[] = {
        { "1121012", "112.img", "1121008", "112.img", "brandish1",
          "Blade Wave", "A wide-range 4th job attack.", nullptr,
          { 8, 500, 10, 200, 15, -390, -600, 70, 20, 600, 0, 0, 0 } },
        { "1221013", "122.img", "1221007", "122.img", "rush2",
          "Divine Judgment", "A wide-range 4th job attack.", nullptr,
          { 6, 500, 15, 400, 15, -500, -300, 500, 300, 600, 0, 0, 0 } },
        { "1321011", "132.img", "1311003", "131.img", "swingP1",
          "Gungnir's Descent", "A wide-range 4th job attack.", nullptr,
          { 10, 330, 10, 200, 15, -330, -440, 330, 60, 600, 0, 0, 0 } },
        { "2121009", "212.img", "2121007", "212.img", "meteor",
          "Inferno Sphere", "A wide-range 4th job attack.", nullptr,
          { 9, 0, 10, 500, 15, -400, -300, 50, 50, 600, 330, 0, 0 } },
        { "2221009", "222.img", "2221007", "222.img", "blizzard",
          "Frozen Orb", "A wide-range 4th job attack.", nullptr,
          { 7, 0, 15, 30, 15, -450, -350, 50, 50, 600, 300, 0, 0 } },
        { "2321010", "232.img", "2321008", "232.img", "genesis",
          "Genesis Nova", "A wide-range 4th job attack.", nullptr,
          { 8, 0, 15, 500, 15, -550, -550, 550, 130, 600, 370, 0, 0 } },
        { "3121011", "312.img", "3121003", "312.img", nullptr,
          "Arrow Barrage", "A wide-range 4th job attack.", nullptr,
          { 8, 500, 12, 200, 15, -400, -435, 400, 20, 600, 0, 8, 1 } },
        { "3221009", "322.img", "3221001", "322.img", nullptr,
          "Piercing Barrage", "A wide-range 4th job attack.", nullptr,
          { 5, 750, 12, 300, 15, -700, -210, -60, 150, 600, 0, 5, 1 } },
        { "4121010", "412.img", "4121008", "412.img", "alert2",
          "Shadow Storm", "A wide-range 4th job attack.", "ninjastorm",
          { 7, 358, 8, 300, 15, -300, -330, 300, 50, 600, 0, 0, 0 } },
        { "4221009", "422.img", "4221007", "422.img", "alert5",
          "Shadow Vortex", "A wide-range 4th job attack.", nullptr,
          { 1, 800, 15, 300, 15, -420, -620, 420, 20, 600, 0, 0, 0 } },
        { "5121011", "512.img", "5121001", "512.img", "dragonstrike",
          "Dragon Fury", "A wide-range 4th job attack.", nullptr,
          { 5, 650, 8, 20, 15, -450, -450, 100, 100, 600, 0, 0, 0 } },
        { "5221011", "522.img", "5201002", "520.img", nullptr,
          "Broadside", "A wide-range 4th job attack.", nullptr,
          { 12, 400, 15, 500, 15, -450, -480, 50, 20, 600, 0, 12, 1 } },
    };

    bool mismatch(const char* key, int64_t want, int64_t got, std::string& why)
    {
        if (want == got)
            return false;
        why = std::string(key) + " want " + std::to_string(want)
            + " got " + std::to_string(got);
        return true;
    }

    bool level_matches(nl::node level, const stats& want, std::string& why)
    {
        if (!present(level))
        {
            why = "level node missing";
            return false;
        }
        if (mismatch("attackCount", want.attack_count,
                     level["attackCount"].get_integer(), why))
            return false;
        if (want.mad > 0)
        {
            // Magic levels scale via mad%; damage must stay absent.
            if (present(level["damage"]))
            {
                why = "magic level carries damage";
                return false;
            }
            if (mismatch("mad", want.mad, level["mad"].get_integer(), why))
                return false;
        }
        else
        {
            if (mismatch("damage", want.damage,
                         level["damage"].get_integer(), why))
                return false;
            if (present(level["mad"]))
            {
                why = "non-magic level carries mad";
                return false;
            }
        }
        if (mismatch("mobCount", want.mob_count,
                     level["mobCount"].get_integer(), why))
            return false;
        if (mismatch("mpCon", want.mp_con, level["mpCon"].get_integer(), why))
            return false;
        if (mismatch("cooltime", want.cooltime,
                     level["cooltime"].get_integer(), why))
            return false;
        const nl::vector2i lt = level["lt"].get_vector();
        if (mismatch("lt.x", want.lt_x, lt.first, why))
            return false;
        if (mismatch("lt.y", want.lt_y, lt.second, why))
            return false;
        const nl::vector2i rb = level["rb"].get_vector();
        if (mismatch("rb.x", want.rb_x, rb.first, why))
            return false;
        if (mismatch("rb.y", want.rb_y, rb.second, why))
            return false;
        if (mismatch("range", want.range, level["range"].get_integer(), why))
            return false;
        if (want.bullet_count > 0
            && mismatch("bulletCount", want.bullet_count,
                        level["bulletCount"].get_integer(), why))
            return false;
        if (want.bullet_count > 0
            && mismatch("bulletConsume", want.bullet_consume,
                        level["bulletConsume"].get_integer(), why))
            return false;
        return true;
    }

    // Resolves action/0; returns "" when the child is absent or not a string.
    std::string action_value(nl::node skill)
    {
        nl::node frame = skill["action"]["0"];
        if (!present(frame) || frame.data_type() != nl::node::type::string)
            return std::string();
        return frame.get_string();
    }

    std::string padded_id(const char* id)
    {
        const std::string digits(id);
        return digits.size() >= 7
            ? digits
            : std::string(7 - digits.size(), '0') + digits;
    }

    void check_string(nl::node strings, const target& t)
    {
        const std::string id = padded_id(t.id);
        nl::node entry = strings["Skill.img"][id];
        check(present(entry), "String.nx Skill.img/" + id + " exists");
        if (!present(entry))
            return;

        check(entry["name"].get_string() == t.name,
              id + " name == " + t.name);
        check(entry["desc"].get_string() == t.desc,
              id + " desc matches the manifest");

        bool every_h = true;
        std::string missing;
        for (int level = 1; level <= 10; ++level)
        {
            const std::string key = "h" + std::to_string(level);
            nl::node hint = entry[key];
            if (!present(hint) || hint.data_type() != nl::node::type::string
                || hint.get_string().empty())
            {
                every_h = false;
                if (missing.empty())
                    missing = key;
            }
        }
        check(every_h, id + " has non-empty h1..h10"
              + (every_h ? "" : " (missing " + missing + ")"));
        check(present(entry["h1"]) && !entry["h1"].get_string().empty(),
              id + " h1 is a non-empty string");
        check(present(entry["h10"]) && !entry["h10"].get_string().empty(),
              id + " h10 is a non-empty string");
    }

    void check_target(nl::node skill, nl::node strings, const target& t)
    {
        const std::string id(t.id);
        std::cout << '[' << id << ' ' << t.name << "]\n";

        nl::node source = skill[t.source_img]["skill"][t.source_id];
        check(present(source), "source " + std::string(t.source_id)
              + " exists at " + t.source_img + "/skill");

        nl::node node = skill[t.img]["skill"][t.id];
        check(present(node),
              id + " exists at " + t.img + "/skill");
        if (!present(node))
            return;

        std::vector<std::string> level_names;
        for (nl::node child : node["level"])
            level_names.push_back(child.name());
        bool named = level_names.size() == 10;
        bool complete = true;
        for (int level = 1; level <= 10; ++level)
        {
            const std::string want = std::to_string(level);
            int hits = 0;
            for (const std::string& name : level_names)
                if (name == want)
                    ++hits;
            if (hits != 1)
                complete = false;
        }
        check(named, id + " has exactly 10 level children"
              + (named ? "" : " (got " + std::to_string(level_names.size())
                  + ")"));
        check(complete, id + " level children are named 1..10");

        bool levels_ok = true;
        std::string detail;
        for (int level = 1; level <= 10; ++level)
        {
            std::string why;
            if (!level_matches(node["level"][std::to_string(level)],
                               t.levels, why))
            {
                levels_ok = false;
                if (detail.empty())
                    detail = "level " + std::to_string(level) + " " + why;
            }
        }
        check(levels_ok, id + " levels 1..10 stats match the manifest"
              + (levels_ok ? "" : " (" + detail + ")"));

        check(present(node["hit"]), id + " has hit");
        check(!present(node["invisible"]), id + " has no invisible");
        check(!present(node["skillType"]), id + " has no skillType");

        nl::node action = node["action"];
        if (present(action))
        {
            const std::string value = action_value(node);
            check(action["0"].data_type() == nl::node::type::string
                  && !value.empty(),
                  id + " action/0 is a non-empty string");
            check(value != "alert2", id + " action/0 is not alert2");
            if (t.action)
                check(value == t.action,
                      id + " action/0 == " + t.action);
        }
        else if (t.action)
            check(false, id + " has action/0 == " + std::string(t.action));
        else
            check(true, id + " has no action child (allowed)");

        if (present(source))
        {
            check(present(source["hit"]),
                  "source " + std::string(t.source_id) + " keeps hit");
            if (t.source_action)
                check(action_value(source) == t.source_action,
                      "source " + std::string(t.source_id)
                      + " keeps action/0 " + t.source_action);
            else
                check(!present(source["action"]),
                      "source " + std::string(t.source_id)
                      + " has no action (unchanged)");
        }

        check_string(strings, t);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: wide-attack-nx Skill.nx String.nx\n";
        return 2;
    }

    nl::file skill;
    nl::file strings;
    try
    {
        skill.open(argv[1]);
        strings.open(argv[2]);
    }
    catch (const std::exception& error)
    {
        std::cerr << "wide-attack-nx: " << error.what() << '\n';
        return 2;
    }

    nl::node skill_root = skill.root();
    nl::node string_root = strings.root();

    for (const target& t : targets)
        check_target(skill_root, string_root, t);

    std::cout << (failures ? "RESULT: FAILED\n" : "RESULT: PASSED\n");
    return failures ? 1 : 0;
}
