#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Character/Look/Clothing.h"
#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

// This verifier reads only NX metadata; bitmap decompression is unnecessary.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    bool is_glove_layer(const std::string& name)
    {
        return name.rfind("glove", 0) == 0 ||
            name.rfind("backGlove", 0) == 0;
    }

    void audit_node(
        nl::node node,
        const std::string& item,
        nl::node zmap,
        std::set<std::string>& observed,
        std::vector<std::string>& failures,
        size_t& parts
    ) {
        if (node.data_type() == nl::node::type::bitmap)
        {
            std::string name = node["z"].get_string();
            if (!is_glove_layer(name)) return;
            ++parts;
            observed.insert(name);

            std::optional<jrc::Clothing::Layer> layer =
                jrc::Clothing::glove_layer_by_name(name);
            if (!layer)
            {
                failures.push_back(item + ": unknown " + name);
                return;
            }
            std::optional<int16_t> expected = jrc::Clothing::glove_z(*layer);
            int16_t actual = static_cast<int16_t>(zmap[name].get_integer());
            if (!expected || *expected != actual)
            {
                failures.push_back(
                    item + ": z mismatch " + name + " expected=" +
                    (expected ? std::to_string(*expected) : "none") +
                    " actual=" + std::to_string(actual)
                );
            }
            return;
        }

        for (nl::node child : node)
        {
            audit_node(child, item, zmap, observed, failures, parts);
        }
    }

    // Mirrors the frame-name parsing of Clothing/BodyDrawInfo: frame children
    // are imgdirs whose names must be pure integers in [0, 255].
    std::optional<uint8_t> parse_frame_name(const std::string& name)
    {
        if (name.empty() || name.front() < '0' || name.front() > '9')
            return std::nullopt;

        char* end = nullptr;
        long value = std::strtol(name.c_str(), &end, 10);
        if (*end != '\0' || value < 0 || value > 255)
            return std::nullopt;

        return static_cast<uint8_t>(value);
    }

    // v83 glove data convention: the attack follow-ups swingO1/swingT2, the
    // transition stances (swingTF/stabTF/swingP2), swingO3, heal and the
    // climb stances legitimately begin past frame 0 (observed up to 2);
    // ladder/rope may omit sprites entirely. Everything else starts at 0.
    bool may_start_late(const std::string& stance)
    {
        return stance == "swingO1" || stance == "swingT2" ||
            stance == "swingTF" || stance == "stabTF" ||
            stance == "swingO3" || stance == "swingP2" ||
            stance == "heal" || stance == "ladder" || stance == "rope";
    }

    void audit_stance_frames(
        nl::node item,
        const std::string& itemname,
        std::map<std::string, uint8_t>& item_first,
        std::vector<std::string>& failures,
        size_t& frames
    ) {
        for (nl::node stance : item)
        {
            const std::string stancename = stance.name();
            if (stancename == "info")
                continue;

            long first = -1;
            for (nl::node framenode : stance)
            {
                if (framenode.data_type() != nl::node::type::none)
                    continue;

                std::optional<uint8_t> frame = parse_frame_name(framenode.name());
                if (!frame)
                {
                    failures.push_back(
                        itemname + "/" + stancename +
                        ": non-numeric frame name '" + framenode.name() + "'"
                    );
                    continue;
                }
                ++frames;
                if (first < 0 || *frame < first)
                    first = *frame;
            }

            if (first < 0)
            {
                if (stancename != "ladder" && stancename != "rope")
                {
                    failures.push_back(
                        itemname + "/" + stancename + ": no frame directories"
                    );
                }
                continue;
            }

            auto known = item_first.find(stancename);
            if (known == item_first.end() || first < known->second)
                item_first[stancename] = static_cast<uint8_t>(first);

            if (first != 0 && !may_start_late(stancename))
            {
                failures.push_back(
                    itemname + "/" + stancename + ": first frame is " +
                    std::to_string(first) + ", expected 0"
                );
            }
            else if (first > 2)
            {
                failures.push_back(
                    itemname + "/" + stancename + ": first frame is " +
                    std::to_string(first) + ", beyond known v83 range"
                );
            }
            else if ((stancename == "swingO1" || stancename == "swingT2") &&
                     first > 1)
            {
                failures.push_back(
                    itemname + "/" + stancename + ": first frame is " +
                    std::to_string(first) + ", expected 0 or 1"
                );
            }
        }
    }
}

int main(int argc, char** argv)
{
    constexpr const char* TARGET_ITEM = "01082003.img";
    const std::string character_path = argc > 1 ? argv[1] : "Character.nx";
    const std::string base_path = argc > 2 ? argv[2] : "Base.nx";
    nl::file character_file(character_path);
    nl::file base_file(base_path);
    nl::node gloves = character_file.root()["Glove"];
    nl::node zmap = base_file.root()["zmap.img"];

    std::set<std::string> observed;
    std::set<std::string> declared;
    std::map<std::string, uint8_t> first_frames;
    std::vector<std::string> failures;
    size_t items = 0;
    size_t parts = 0;
    size_t frames = 0;
    size_t target_parts = 0;
    long target_swing_o1 = -1;
    bool target_found = false;
    std::set<std::string> target_layers;

    if (jrc::Clothing::glove_layer_by_name("futureUnknownGloveLayer"))
    {
        failures.push_back("unknown glove labels must be rejected for safe skip");
    }

    for (nl::node entry : zmap)
    {
        std::string name = entry.name();
        if (!is_glove_layer(name)) continue;
        declared.insert(name);
        std::optional<jrc::Clothing::Layer> layer =
            jrc::Clothing::glove_layer_by_name(name);
        std::optional<int16_t> expected = layer
            ? jrc::Clothing::glove_z(*layer)
            : std::nullopt;
        int16_t actual = static_cast<int16_t>(entry.get_integer());
        if (!expected || *expected != actual)
        {
            failures.push_back(
                "Base zmap mismatch " + name + " expected=" +
                (expected ? std::to_string(*expected) : "none") +
                " actual=" + std::to_string(actual)
            );
        }
    }

    for (nl::node item : gloves)
    {
        ++items;
        size_t item_parts = 0;
        std::set<std::string> item_layers;
        audit_node(item, item.name(), zmap, item_layers, failures, item_parts);
        observed.insert(item_layers.begin(), item_layers.end());
        parts += item_parts;

        std::map<std::string, uint8_t> item_first;
        audit_stance_frames(item, item.name(), item_first, failures, frames);
        for (const auto& entry : item_first)
        {
            auto known = first_frames.find(entry.first);
            if (known == first_frames.end() || entry.second < known->second)
                first_frames[entry.first] = entry.second;
        }
        if (item.name() == TARGET_ITEM)
        {
            target_found = true;
            target_parts = item_parts;
            target_layers = item_layers;
            auto swing = item_first.find("swingO1");
            if (swing != item_first.end())
                target_swing_o1 = swing->second;
        }
    }

    constexpr size_t EXPECTED_LABELS = 16;
    if (declared.size() != EXPECTED_LABELS)
    {
        std::string labels;
        for (const std::string& label : declared)
        {
            if (!labels.empty()) labels += ",";
            labels += label;
        }
        failures.push_back(
            "expected 16 Base glove z labels, found " +
            std::to_string(declared.size()) + ": " + labels
        );
    }
    if (items == 0 || parts == 0)
    {
        failures.push_back("no glove frame metadata was audited");
    }
    if (!target_found || target_parts == 0)
    {
        failures.push_back(
            std::string(TARGET_ITEM) + " is missing or has no glove bitmap parts"
        );
    }
    if (!target_layers.count("backGlove"))
    {
        failures.push_back(
            std::string(TARGET_ITEM) + " does not exercise backGlove"
        );
    }
    if (first_frames.empty() || frames == 0)
    {
        failures.push_back("no stance frame metadata was audited");
    }
    else
    {
        auto swing_o1 = first_frames.find("swingO1");
        auto swing_t2 = first_frames.find("swingT2");
        if (swing_o1 == first_frames.end() || swing_t2 == first_frames.end())
        {
            failures.push_back(
                "swingO1 and swingT2 stances were not exercised"
            );
        }
    }
    if (target_swing_o1 != 1)
    {
        failures.push_back(
            std::string(TARGET_ITEM) + " swingO1 must start at frame 1"
        );
    }

    if (!failures.empty())
    {
        for (const std::string& failure : failures) std::cerr << failure << '\n';
        return 1;
    }
    std::cout << "glove layering audit passed: " << items << " items, "
              << parts << " parts, " << declared.size() << " z labels; "
              << TARGET_ITEM << "=" << target_parts << " parts; "
              << first_frames.size() << " stances, " << frames
              << " frames, swingO1 starts at " << target_swing_o1 << '\n';
    return 0;
}
