// Audits the 079MAX4-2023 Skill.nx data contracts behind the Final Attack
// follow-up attack and the Combo Attack orb rendering:
//  - 1001005 (Slash Blast) must keep its finalAttack child table naming the
//    six warrior FA variants, so the client can map primary skill + learned
//    book + weapon family to a follow-up attack skill id.
//  - The FA skill nodes that exist in this dataset (1100002, 1200003,
//    1300002) must carry prop/damage per level with attackCount=2 and
//    mobCount=8 on EVERY level (patch_final_attack_counts_nx.py), so the
//    follow-up hits the full 2-mob spread before max level too.
//    The three variants 079MAX4 dropped (1100003, 1200002, 1300003) are
//    grafted from the v83 archive by inject_final_attack_variants_nx.py; they
//    keep their own icon bitmaps plus the resident sibling's per-level child
//    set (079MAX4 balance: level 30 60% / 350% with the lt/rb hitbox
//    vectors), and the archer / Cygnus FA skills (3100001, 3200001, 11101002,
//    13101002) plus the Dawn Warrior 1st-job trigger tables
//    (11001002/11001003 -> 11101002 30/40) must carry the same 2-hits-8-mobs
//    contract.  The three ranged variants (3100001/3200001/13101002) must
//    also carry bulletCount=2 on every level: the client's Attack::RANGED
//    path takes the damage-line count from bulletCount, not attackCount, so
//    without it the archer FA still lands a single line.  11101002 is the
//    Dawn Warrior sword FA (Attack::CLOSE) and must carry no bulletCount.
//  - Every skill id in the archive must resolve to the image named by its
//    prefix (7 digits -> 3, 8 digits -> 4), the rule string_format::skill_image
//    implements for the client's library lookups.
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
        // v83 image names use the 3-digit job prefix; 8-digit (Cygnus) skill
        // ids live in the 4-digit image (11101002 -> 1110.img).
        const std::string sid = id;
        return root[sid.substr(0, sid.size() > 7 ? 4 : 3) + ".img"]["skill"][id];
    }

    void check_every_level_counters(nl::node node, const char* id)
    {
        const std::string label = std::string(id) + " ";
        nl::node levels = node["level"];
        bool ok = present(levels) && levels.size() == 30;
        for (auto lv : levels)
        {
            ok = ok && is_int(lv["attackCount"])
                && lv["attackCount"].get_integer() == 2
                && is_int(lv["mobCount"])
                && lv["mobCount"].get_integer() == 8;
        }
        check(ok, label + "all 30 levels grant 2 hits on 8 mobs");
    }

    void check_every_level_bullet(nl::node node, const char* id, bool wanted)
    {
        // Ranged FA variants: Player::prepare_attack(true) returns
        // Attack::RANGED for bow/crossbow, and Skill.cpp apply_stats then
        // takes hitcount from bulletCount (SkillData defaults it to 1), so
        // attackCount=2 alone still fires a single damage line. Melee FA
        // skills (11101002, Dawn Warrior sword) must carry no bulletCount.
        const std::string label = std::string(id) + " ";
        nl::node levels = node["level"];
        bool ok = present(levels) && levels.size() == 30;
        for (auto lv : levels)
        {
            const bool has = is_int(lv["bulletCount"]);
            ok = ok && (wanted ? has && lv["bulletCount"].get_integer() == 2
                               : !has);
        }
        check(ok, label
            + (wanted ? "all 30 levels carry bulletCount=2 (ranged FA)"
                      : "carries no bulletCount (melee FA)"));
    }

    void check_image_prefixes(nl::node root)
    {
        // string_format::skill_image() derives the image from the id: three
        // digits for 7-digit ids, four for the 8-digit Cygnus/Aran ones. The
        // whole archive must follow that rule or skill lookups silently miss.
        size_t images = 0;
        size_t skills = 0;
        size_t bad = 0;
        std::string sample;
        for (auto img : root)
        {
            const std::string name = img.name();
            if (name.size() != 7 && name.size() != 8)
                continue;
            nl::node entries = img["skill"];
            if (!present(entries))
                continue;
            ++images;
            for (auto entry : entries)
            {
                const std::string id = entry.name();
                ++skills;
                const std::string want =
                    id.substr(0, id.size() > 7 ? 4 : 3) + ".img";
                if (want != name)
                {
                    if (bad++ == 0)
                        sample = name + "/" + id;
                }
            }
        }
        check(images >= 70 && skills >= 480,
            "archive has the expected skill images (" + std::to_string(images)
                + " images, " + std::to_string(skills) + " skills)");
        check(bad == 0, "every skill id resolves to its image name"
            + (bad ? ", first miss " + sample : ""));
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

    check_image_prefixes(skill);

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

    std::cout << "FA skill nodes grafted from the v83 archive (079 gap fill)\n";
    const char* grafted_ids[] = {"1100003", "1200002", "1300003"};
    for (const char* fa : grafted_ids)
    {
        const std::string label = std::string(fa) + " ";
        nl::node node = find_skill(skill, fa);
        check(present(node), label + "skill node exists");
        check(node.size() == 7 && node["level"].size() == 30,
            label + "keeps icons/hit/req/skillType and 30 levels");
        check(node["icon"].data_type() == nl::node::type::bitmap
                && node["iconDisabled"].data_type() == nl::node::type::bitmap
                && node["iconMouseOver"].data_type() == nl::node::type::bitmap,
            label + "carries the three icon bitmaps");
        check(is_int(node["skillType"]), label + "keeps skillType");
        nl::node lv1 = node["level"]["1"];
        nl::node lv30 = node["level"]["30"];
        check(is_int(lv1["prop"]) && lv1["prop"].get_integer() == 2
                && is_int(lv1["damage"]) && lv1["damage"].get_integer() == 105,
            label + "level 1 keeps the 2% / 105% curve");
        check(is_int(lv30["prop"]) && lv30["prop"].get_integer() == 60
                && is_int(lv30["damage"]) && lv30["damage"].get_integer() == 350,
            label + "level 30 carries the sibling 60% / 350% balance");
        nl::node lt = lv30["lt"];
        nl::node rb = lv30["rb"];
        check(lt.data_type() == nl::node::type::vector
                && rb.data_type() == nl::node::type::vector
                && lt.get_vector() == (nl::vector2i{-300, -300})
                && rb.get_vector() == (nl::vector2i{300, 300}),
            label + "level 30 carries the sibling lt/rb hitbox vectors");
        check_every_level_counters(node, fa);
    }

    std::cout << "Dawn Warrior 1st-job FA trigger tables (079 gap fill)\n";
    for (const char* id : {"11001002", "11001003"})
    {
        const std::string label = std::string("finalAttack ") + id + " ";
        nl::node table = find_skill(skill, id)["finalAttack"];
        check(table.size() == 1 && present(table["11101002"]),
            label + "names exactly 11101002");
        check(is_int(table["11101002"]["0"])
                && table["11101002"]["0"].get_integer() == 30
                && is_int(table["11101002"]["1"])
                && table["11101002"]["1"].get_integer() == 40,
            label + "carries prop 30 / damage 40");
    }

    std::cout << "Archer / Cygnus FA counters\n";
    for (const char* id : {"3100001", "3200001", "11101002", "13101002"})
        check_every_level_counters(find_skill(skill, id), id);

    std::cout << "Ranged FA damage-line count (bulletCount)\n";
    for (const char* id : {"3100001", "3200001", "13101002"})
        check_every_level_bullet(find_skill(skill, id), id, true);
    check_every_level_bullet(find_skill(skill, "11101002"), "11101002", false);

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
        check(is_int(lv1["attackCount"]) && lv1["attackCount"].get_integer() == 2
                && is_int(lv1["mobCount"]) && lv1["mobCount"].get_integer() == 8,
            label + "level 1 grants 2 hits on 8 mobs");

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
