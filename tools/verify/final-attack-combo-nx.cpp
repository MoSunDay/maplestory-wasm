// Audits the 079MAX4-2023 Skill.nx data contracts behind the Final Attack
// follow-up attack and the Combo Attack orb rendering:
//  - 1001005 (Slash Blast) must keep its finalAttack child table naming the
//    six warrior FA variants, so the client can map primary skill + learned
//    book + weapon family to a follow-up attack skill id.
//  - The FA skill nodes that exist in this dataset (1100002, 1200003,
//    1300002) must carry prop/damage per level with attackCount/mobCount
//    only on the max level, matching SkillData's default of 1 hit/1 mob.
//    The three absent variants (1100003, 1200002, 1300003) must stay absent
//    so find_skill() filters them out instead of sending empty skills.
//  - Combo Attack (1111002) and Advanced Combo (1120003) must expose their
//    'state' node with six direct bitmap frames: the static aura frame "0"
//    plus five pulsing orb frames "1".."5" with delay/a0/a1 fade props.
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

    bool is_int(nl::node n)
    {
        return n.data_type() == nl::node::type::integer;
    }

    nl::node find_skill(nl::node root, const char* id)
    {
        return root[std::string(id, 3) + ".img"]["skill"][id];
    }

    void check_combo_state(nl::node root, const char* id)
    {
        const std::string label = std::string(id) + " state ";
        nl::node state = find_skill(root, id)["state"];
        check(present(state), label + "node exists");

        bool frames_ok = true;
        for (const char* frame : {"0", "1", "2", "3", "4", "5"})
        {
            nl::node f = state[frame];
            if (f.data_type() != nl::node::type::bitmap)
                frames_ok = false;
        }
        check(frames_ok, label + "has bitmap frames 0..5");

        size_t count = 0;
        for (auto child : state)
        {
            (void)child;
            ++count;
        }
        check(count == 6, label + "has exactly six frames");

        nl::node aura = state["0"];
        check(!is_int(aura["delay"]),
            label + "aura frame 0 is static (no delay)");

        bool orb_props_ok = true;
        for (const char* frame : {"1", "2", "3", "4", "5"})
        {
            nl::node f = state[frame];
            orb_props_ok = orb_props_ok && is_int(f["delay"])
                && is_int(f["a0"]) && is_int(f["a1"]);
        }
        check(orb_props_ok, label + "orb frames carry delay/a0/a1 fade");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: final-attack-combo-nx Skill.nx\n";
        return 2;
    }
    nl::file file(argv[1]);
    nl::node skill = file.root();

    std::cout << "finalAttack trigger table\n";
    nl::node table = find_skill(skill, "1001005")["finalAttack"];
    check(present(table), "1001005 exposes finalAttack");
    const char* fa_ids[] = {
        "1100002", "1100003", "1200002", "1200003", "1300002", "1300003"
    };
    for (const char* fa : fa_ids)
        check(present(table[fa]),
            std::string("table names FA variant ") + fa);

    std::cout << "FA skill nodes present in this dataset\n";
    const char* present_ids[] = {"1100002", "1200003", "1300002"};
    for (const char* fa : present_ids)
        check(present(find_skill(skill, fa)),
            std::string("skill node exists: ") + fa);

    std::cout << "FA skill nodes absent in this dataset (regression)\n";
    const char* absent_ids[] = {"1100003", "1200002", "1300003"};
    for (const char* fa : absent_ids)
        check(!present(find_skill(skill, fa)),
            std::string("skill node absent: ") + fa);

    std::cout << "FA level stats (prop/damage/counters)\n";
    for (const char* fa : present_ids)
    {
        const std::string label = std::string(fa) + " ";
        nl::node node = find_skill(skill, fa);

        nl::node lv1 = node["level"]["1"];
        nl::node lv30 = node["level"]["30"];
        check(present(lv1) && present(lv30), label + "has levels 1 and 30");

        check(is_int(lv1["prop"]) && lv1["prop"].get_integer() == 2,
            label + "level 1 prop is 2%");
        check(is_int(lv1["damage"]) && lv1["damage"].get_integer() > 100,
            label + "level 1 damage above 100%");
        check(!is_int(lv1["attackCount"]) && !is_int(lv1["mobCount"]),
            label + "level 1 relies on default 1 hit / 1 mob");

        check(is_int(lv30["prop"]) && lv30["prop"].get_integer() == 60,
            label + "level 30 prop is 60%");
        check(is_int(lv30["damage"]) && lv30["damage"].get_integer() >= 350,
            label + "level 30 damage at least 350%");
        check(is_int(lv30["attackCount"])
                && lv30["attackCount"].get_integer() == 2
                && is_int(lv30["mobCount"])
                && lv30["mobCount"].get_integer() == 8,
            label + "level 30 grants 2 hits on 8 mobs");
    }

    std::cout << "grafted Panic / Axe Coma nodes (079 gap fill)\n";
    for (const char* id : {"1111003", "1111004"})
    {
        const std::string label = std::string(id) + " ";
        nl::node node = find_skill(skill, id);
        check(present(node), label + "skill node exists");
        nl::node lv30 = node["level"]["30"];
        check(is_int(lv30["damage"]) && lv30["damage"].get_integer() > 0,
            label + "level 30 damage is positive");
        check(is_int(lv30["attackCount"])
                && lv30["attackCount"].get_integer() == 6,
            label + "level 30 attacks six times");
        check(is_int(lv30["mobCount"])
                && lv30["mobCount"].get_integer() == 1,
            label + "level 30 stays single target");
        check(is_int(lv30["prop"]), label + "level 30 carries a status prop");
    }

    nl::node coma_axe = find_skill(skill, "1111006");
    check(present(coma_axe), "1111006 skill node exists");
    nl::node coma_axe_30 = coma_axe["level"]["30"];
    check(is_int(coma_axe_30["attackCount"])
            && coma_axe_30["attackCount"].get_integer() == 8,
        "1111006 level 30 attacks eight times");
    check(is_int(coma_axe_30["mobCount"])
            && coma_axe_30["mobCount"].get_integer() == 15,
        "1111006 level 30 hits 15 mobs");
    check(is_int(coma_axe_30["hpCon"])
            && coma_axe_30["hpCon"].get_integer() == 25,
        "1111006 level 30 costs 25% HP");

    std::cout << "combo orb state animations\n";
    check_combo_state(skill, "1111002");
    check_combo_state(skill, "1120003");

    return failures == 0 ? 0 : 1;
}
