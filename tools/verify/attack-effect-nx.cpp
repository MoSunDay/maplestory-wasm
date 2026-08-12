#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Gameplay/Combat/Effects/AttackEffectSelection.h"
#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// The audit only inspects node metadata. Supplying this constructor avoids
// linking the runtime bitmap decoder and keeps the verifier read-only.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
}

namespace
{
    bool has_frames(nl::node node)
    {
        return node["0"].data_type() == nl::node::type::bitmap;
    }

    std::vector<nl::node> animation_variants(nl::node parent)
    {
        std::vector<nl::node> variants;
        for (auto child : parent)
        {
            if (has_frames(child))
                variants.push_back(child);
        }
        return variants;
    }

    uint8_t maximum(nl::node levels, const char* property)
    {
        uint8_t value = 1;
        for (auto level : levels)
        {
            value = std::max<uint8_t>(
                value,
                static_cast<uint8_t>(level[property].get_integer(1)));
        }
        return value;
    }

    bool numeric_sequence(const std::vector<nl::node>& variants)
    {
        for (size_t index = 0; index < variants.size(); ++index)
        {
            if (variants[index].name() != std::to_string(index))
                return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "Skill.nx";
    nl::file file(path);
    nl::node root = file.root();

    size_t skills_with_hits = 0;
    size_t indexed_by_hit = 0;
    size_t indexed_by_target = 0;
    size_t character_level_hit_groups = 0;
    std::set<int> positions;
    std::vector<std::string> failures;

    for (auto book : root)
    {
        for (auto skill : book["skill"])
        {
            auto variants = animation_variants(skill["hit"]);
            size_t nested_variant_count = 0;
            for (auto character_level : skill["CharLevel"])
            {
                auto nested = animation_variants(character_level["hit"]);
                if (nested.empty())
                    continue;

                ++character_level_hit_groups;
                if (!numeric_sequence(nested))
                {
                    failures.push_back(
                        skill.name() + "/CharLevel/" + character_level.name()
                        + ": hit branches are not contiguous from zero");
                }
                if (nested_variant_count != 0 && nested_variant_count != nested.size())
                {
                    failures.push_back(
                        skill.name() + ": CharLevel hit branch counts differ");
                }
                nested_variant_count = std::max(nested_variant_count, nested.size());
            }

            if (variants.empty() && nested_variant_count == 0)
                continue;

            ++skills_with_hits;
            if (!variants.empty() && !numeric_sequence(variants))
                failures.push_back(skill.name() + ": hit branches are not contiguous from zero");

            uint8_t attack_count = std::max(
                maximum(skill["level"], "attackCount"),
                maximum(skill["level"], "bulletCount"));
            uint8_t mob_count = maximum(skill["level"], "mobCount");
            // CharLevel branches follow weapon-handedness semantics in the
            // client. Only direct hit branches are damage/target indexed.
            auto indexed = nested_variant_count == 0
                ? jrc::attack_effect::indexed_hit(variants.size(), attack_count, mob_count)
                : std::nullopt;
            if (indexed == jrc::attack_effect::HitIndex::HIT)
                ++indexed_by_hit;
            else if (indexed == jrc::attack_effect::HitIndex::TARGET)
                ++indexed_by_target;

            for (nl::node variant : variants)
            {
                int position = static_cast<int>(variant["pos"].get_integer(0));
                positions.insert(position);
                if (position < 0 || position > 4)
                {
                    failures.push_back(
                        skill.name() + "/hit/" + variant.name()
                        + ": unsupported pos=" + std::to_string(position));
                }
            }
        }
    }

    const std::vector<std::string> finishers{
        "1111003", "1111004", "1111005", "1111006"
    };
    for (const std::string& id : finishers)
    {
        nl::node skill = root[id.substr(0, 3) + ".img"]["skill"][id];
        for (int orbs = 1; orbs <= 10; ++orbs)
        {
            if (!has_frames(skill["finish"][orbs]))
                failures.push_back(id + ": missing finish/" + std::to_string(orbs));
        }
    }

    nl::node charged_blow = root["121.img"]["skill"]["1211002"]["effect"];
    for (int element : { 1, 2, 3, 5 })
    {
        if (!has_frames(charged_blow[element]))
            failures.push_back("1211002: missing effect/" + std::to_string(element));
    }

    std::cout << "skills_with_hits=" << skills_with_hits
              << " character_level_hit_groups=" << character_level_hit_groups
              << " indexed_by_hit=" << indexed_by_hit
              << " indexed_by_target=" << indexed_by_target
              << " positions=";
    for (int position : positions)
        std::cout << position << ',';
    std::cout << '\n';

    for (const std::string& failure : failures)
        std::cerr << "FAIL " << failure << '\n';
    return failures.empty() ? 0 : 1;
}
