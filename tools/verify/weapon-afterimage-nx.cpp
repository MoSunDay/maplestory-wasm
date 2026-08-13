#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "Character/Look/Afterimage/Resolver.h"
#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// The audit inspects node metadata only, so decoding bitmap payloads is not
// needed. Defining the constructor keeps the verifier small and read-only.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    std::optional<int16_t> numeric_name(const std::string& name)
    {
        int16_t value = 0;
        auto result = std::from_chars(
            name.data(),
            name.data() + name.size(),
            value
        );
        if (result.ec != std::errc{} || result.ptr != name.data() + name.size())
            return std::nullopt;
        return value;
    }

    bool has_animation(nl::node stance)
    {
        for (nl::node cue : stance)
        {
            std::optional<int16_t> frame = numeric_name(cue.name());
            if (frame && *frame >= 0 && *frame < 255 &&
                cue[0].data_type() == nl::node::type::bitmap)
            {
                return true;
            }
        }
        return false;
    }

    bool has_renderable_animation(nl::node stance)
    {
        for (nl::node cue : stance)
        {
            std::optional<int16_t> frame = numeric_name(cue.name());
            if (!frame || *frame < 0 || *frame >= 255 ||
                cue[0].data_type() != nl::node::type::bitmap)
            {
                continue;
            }

            nl::bitmap image = cue[0];
            if (image.width() > 0 && image.height() > 0)
                return true;
        }
        return false;
    }

    bool cues_reachable(nl::node stance, nl::node body_stance)
    {
        for (nl::node cue : stance)
        {
            std::optional<int16_t> frame = numeric_name(cue.name());
            if (frame && *frame >= 0 && *frame < 255 &&
                cue[0].data_type() == nl::node::type::bitmap &&
                !body_stance[*frame])
            {
                return false;
            }
        }
        return true;
    }

    std::vector<std::string> attack_stances(int attack)
    {
        switch (attack)
        {
        case 1:
            return { "stabO1", "stabO2", "swingO1", "swingO2", "swingO3" };
        case 2:
            return { "stabT1", "swingP1" };
        case 5:
            return { "stabO1", "stabO2", "swingT1", "swingT2", "swingT3" };
        default:
            return {};
        }
    }

    std::vector<int16_t> stance_tiers(nl::node tiers, const std::string& stance)
    {
        std::vector<int16_t> available;
        for (nl::node tier : tiers)
        {
            std::optional<int16_t> bucket = numeric_name(tier.name());
            if (bucket && *bucket >= 0 && has_animation(tier[stance]))
                available.push_back(*bucket);
        }
        return available;
    }
}

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "Character.nx";
    nl::file file(path);
    nl::node root = file.root();

    size_t weapons = 0;
    size_t checked_stances = 0;
    size_t capped_tiers = 0;
    std::vector<std::string> failures;

    for (nl::node weapon : root["Weapon"])
    {
        nl::node info = weapon["info"];
        std::string name = info["afterImage"].get_string();
        std::vector<std::string> stances = attack_stances(
            static_cast<int>(info["attack"].get_integer())
        );
        if (name.empty() || stances.empty())
            continue;

        ++weapons;
        int16_t requested = static_cast<int16_t>(
            info["reqLevel"].get_integer() / 10
        );
        nl::node tiers = root["Afterimage"][name + ".img"];
        for (const std::string& stance : stances)
        {
            std::optional<int16_t> selected = jrc::afterimage::select_level_bucket(
                requested,
                stance_tiers(tiers, stance)
            );
            if (!selected)
            {
                failures.push_back(
                    weapon.name() + ": missing " + name + "/" + stance
                );
                continue;
            }

            ++checked_stances;
            if (*selected < requested)
                ++capped_tiers;

            nl::node source = tiers[*selected][stance];
            if (!has_renderable_animation(source))
            {
                failures.push_back(
                    weapon.name() + ": empty bitmap " + name + "/" +
                    std::to_string(*selected) + "/" + stance
                );
            }
            if (!cues_reachable(source, root["00002000.img"][stance]))
            {
                failures.push_back(
                    weapon.name() + ": unreachable cue " + name + "/" +
                    std::to_string(*selected) + "/" + stance
                );
            }
            if (source["lt"].data_type() != nl::node::type::vector ||
                source["rb"].data_type() != nl::node::type::vector)
            {
                failures.push_back(
                    weapon.name() + ": invalid range " + name + "/" +
                    std::to_string(*selected) + "/" + stance
                );
            }
        }
    }

    std::cout << "weapons=" << weapons
              << " checked_stances=" << checked_stances
              << " capped_tiers=" << capped_tiers << '\n';
    for (const std::string& failure : failures)
        std::cerr << "FAIL " << failure << '\n';

    return weapons > 0 && failures.empty() ? 0 : 1;
}
