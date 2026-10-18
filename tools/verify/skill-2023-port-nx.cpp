// Audits the 2023-annual-edition (079MAX4) Skill.nx produced by
// tools/nx/merge_skill_nx.py: wrapper inventory, binary-search ordering,
// tuned 2023 stat values, v83 GM carryover, and canvas resolution.
#include <cstdint>
#include <cstring>
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
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
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

    long bitmap_nodes = 0;
    long audio_nodes = 0;
    long total_nodes = 0;
    long order_violations = 0;

    void walk(nl::node n)
    {
        ++total_nodes;
        if (n.data_type() == nl::node::type::bitmap)
            ++bitmap_nodes;
        if (n.data_type() == nl::node::type::audio)
            ++audio_nodes;
        const auto count = n.size();
        // Integer operator[] does a name lookup; walk via node iterators.
        auto it = n.begin();
        std::string prev;
        for (auto i = 0u; i < count; ++i, ++it)
        {
            const auto child = *it;
            if (i > 0 && prev > child.name())
                ++order_violations;
            prev = child.name();
            walk(child);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: skill-2023-port-nx Skill.nx\n";
        return 2;
    }
    nl::file file(argv[1]);
    nl::node skill = file.root();

    const char* wrappers[] = {
        "000.img", "100.img", "200.img", "210.img", "300.img", "400.img",
        "500.img", "800.img", "900.img", "910.img", "1000.img", "1300.img",
        "2000.img", "2112.img", "BFSkill.img", "ItemSkill.img",
        "MCGuardian.img", "MCSkill.img", "MobSkill.img",
    };
    std::cout << "[wrappers]\n";
    for (auto w : wrappers)
        check(skill[w].size() > 0, std::string("root has ") + w);
    check(skill.size() == 76, "root wrapper count == 76 (75 of 2023 + 910)");

    std::cout << "[2023 tuned values]\n";
    nl::node nimble = skill["000.img"]["skill"]["0001002"]["level"]["1"];
    check(nimble["mpCon"].get_integer() == 4, "0001002 lv1 mpCon == 4");
    check(nimble["time"].get_integer() == 4, "0001002 lv1 time == 4");
    check(nimble["speed"].get_integer() == 10, "0001002 lv1 speed == 10");
    check(nimble["cooltime"].get_integer() == 3600, "0001002 lv1 cooltime == 3600 (1h, ops override)");
    nl::node hp_up = skill["100.img"]["skill"]["1000001"]["level"]["1"];
    check(hp_up["x"].get_integer() == 4, "1000001 lv1 x == 4");
    check(hp_up["y"].get_integer() == 3, "1000001 lv1 y == 3");
    nl::node mob_buff = skill["MobSkill.img"]["100"]["level"]["1"];
    check(mob_buff["x"].get_integer() == 115, "MobSkill/100 lv1 x == 115");
    check(mob_buff["mpCon"].get_integer() == 5, "MobSkill/100 lv1 mpCon == 5");
    check(mob_buff["interval"].get_integer() == 40, "MobSkill/100 lv1 interval == 40");

    std::cout << "[v83 GM carryover]\n";
    nl::node gm = skill["910.img"]["skill"]["9101000"];
    check(gm.size() > 0, "910.img carries GM skill 9101000");
    check(skill["910.img"]["skill"].size() == 7, "910.img GM skill set intact (7)");

    std::cout << "[tree integrity]\n";
    walk(skill);
    check(order_violations == 0, "children sorted for binary search");
    std::cout << "  info  nodes=" << total_nodes
              << " bitmaps=" << bitmap_nodes
              << " audios=" << audio_nodes << '\n';
    check(bitmap_nodes > 14000, "bitmap node count > 14000");
    nl::node icon = skill["000.img"]["skill"]["0001000"]["icon"];
    check(icon.data_type() == nl::node::type::bitmap, "0001000 icon is canvas");
    check(icon.get_bitmap().width() == 32
          && icon.get_bitmap().height() == 32, "0001000 icon 32x32");

    std::cout << (failures ? "RESULT: FAILED\n" : "RESULT: PASSED\n");
    return failures ? 1 : 0;
}
