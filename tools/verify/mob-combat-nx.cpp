#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// This read-only audit only needs bitmap metadata, not payload decoding.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
}

namespace
{
    bool has_animation(nl::node source)
    {
        for (nl::node frame : source)
            if (frame.data_type() == nl::node::type::bitmap) return true;
        return false;
    }

    bool has_range(nl::node info)
    {
        nl::node range = info["range"];
        return (range["lt"].data_type() == nl::node::type::vector
                && range["rb"].data_type() == nl::node::type::vector)
            || (range["sp"].data_type() == nl::node::type::vector
                && range["r"].get_integer(0) > 0);
    }

    nl::node linked_visual(nl::node root, nl::node mob)
    {
        std::string link = mob["info"]["link"].get_string();
        if (link.empty())
        {
            int64_t numeric = mob["info"]["link"].get_integer(0);
            if (numeric > 0)
                link = std::to_string(numeric);
        }
        nl::node linked = link.empty() ? nl::node() : root[link + ".img"];
        return linked ? linked : mob;
    }

    bool supported_skill(int id)
    {
        static const std::set<int> ids{
            100, 101, 102, 103, 110, 111, 112, 113, 114, 115,
            120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
            131, 132, 133, 134, 135, 136,
            140, 141, 142, 143, 144, 145,
            150, 151, 152, 153, 154, 155, 156, 200
        };
        return ids.count(id) != 0;
    }

    const std::set<std::string> EXPECTED_DISABLED_BINDINGS{
        "9300268.img: 110/1/action1",
        "9300268.img: 112/4/action1",
        "9300268.img: 113/2/action1",
        "9300268.img: 140/4/action1",
        "9300268.img: 200/83/action1",
        "9400630.img: 128/1/action2",
        "9400631.img: 128/2/action3"
    };
}

int main(int argc, char** argv)
{
    const std::string mob_path = argc > 1 ? argv[1] : "Mob.nx";
    const std::string skill_path = argc > 2 ? argv[2] : "Skill.nx";
    nl::file mob_file(mob_path);
    nl::file skill_file(skill_path);
    nl::node mobs = mob_file.root();
    nl::node mob_skills = skill_file.root()["MobSkill.img"];

    size_t attacks = 0;
    size_t usable_attacks = 0;
    size_t disabled_attacks = 0;
    size_t bindings = 0;
    std::set<int> bound_skill_ids;
    std::vector<std::string> failures;
    std::vector<std::string> disabled_bindings;
    std::set<std::string> observed_disabled_bindings;

    for (nl::node mob : mobs)
    {
        nl::node visual = linked_visual(mobs, mob);
        for (int action = 1; action <= 9; ++action)
        {
            const std::string name = "attack" + std::to_string(action);
            nl::node source = mob[name] ? mob[name] : visual[name];
            nl::node animation = visual[name] ? visual[name] : mob[name];
            if (!source && !animation)
                continue;
            ++attacks;
            if (has_animation(animation) && has_range(source["info"]))
                ++usable_attacks;
            else
                ++disabled_attacks;
        }

        nl::node skills = mob["info"]["skill"];
        if (!skills)
            skills = visual["info"]["skill"];
        for (nl::node binding : skills)
        {
            ++bindings;
            int id = static_cast<int>(binding["skill"].get_integer(0));
            int level = static_cast<int>(binding["level"].get_integer(0));
            int action = static_cast<int>(binding["action"].get_integer(0));
            bound_skill_ids.insert(id);
            const std::string prefix = mob.name() + ": " + std::to_string(id)
                + '/' + std::to_string(level) + "/action" + std::to_string(action);
            if (!supported_skill(id))
                failures.push_back(prefix + " is not implemented by MobSkill");
            if (level <= 0 || action < 1 || action > 9)
                failures.push_back(prefix + " has invalid action/level");
            const std::string action_name = "skill" + std::to_string(action);
            nl::node action_visual = visual[action_name]
                ? visual[action_name] : mob[action_name];
            if (!has_animation(action_visual))
            {
                disabled_bindings.push_back(prefix + " has no declared skill animation");
                observed_disabled_bindings.insert(prefix);
                if (EXPECTED_DISABLED_BINDINGS.count(prefix) == 0)
                    failures.push_back(prefix + " is an unexpected missing skill animation");
            }
            if (!mob_skills[std::to_string(id)]["level"][std::to_string(level)])
                failures.push_back(prefix + " has no MobSkill level data");
        }
    }

    for (const std::string& expected : EXPECTED_DISABLED_BINDINGS)
        if (observed_disabled_bindings.count(expected) == 0)
            failures.push_back(expected + " expected disabled binding changed");

    std::cout << "attacks=" << attacks
              << " usable_attacks=" << usable_attacks
              << " explicitly_disabled_attacks=" << disabled_attacks
              << " skill_bindings=" << bindings
              << " explicitly_disabled_bindings=" << disabled_bindings.size()
              << " bound_skill_ids=";
    for (int id : bound_skill_ids)
        std::cout << id << ',';
    std::cout << '\n';
    for (const std::string& failure : failures)
        std::cerr << "FAIL " << failure << '\n';
    for (const std::string& disabled : disabled_bindings)
        std::cerr << "WARN " << disabled << "; binding is disabled\n";
    return attacks > 0 && usable_attacks > 0 && bindings > 0 && failures.empty() ? 0 : 1;
}
