#include <cstdint>
#include <iostream>
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
            if (name.rfind("glove", 0) != 0) return;
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
}

int main(int argc, char** argv)
{
    const std::string character_path = argc > 1 ? argv[1] : "Character.nx";
    const std::string base_path = argc > 2 ? argv[2] : "Base.nx";
    nl::file character_file(character_path);
    nl::file base_file(base_path);
    nl::node gloves = character_file.root()["Glove"];
    nl::node zmap = base_file.root()["zmap.img"];

    std::set<std::string> observed;
    std::set<std::string> declared;
    std::vector<std::string> failures;
    size_t items = 0;
    size_t parts = 0;

    for (nl::node entry : zmap)
    {
        std::string name = entry.name();
        if (name.rfind("glove", 0) != 0) continue;
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
        audit_node(item, item.name(), zmap, observed, failures, parts);
    }

    constexpr size_t EXPECTED_LABELS = 14;
    if (declared.size() != EXPECTED_LABELS)
    {
        std::string labels;
        for (const std::string& label : declared)
        {
            if (!labels.empty()) labels += ",";
            labels += label;
        }
        failures.push_back(
            "expected 14 Base glove z labels, found " +
            std::to_string(declared.size()) + ": " + labels
        );
    }
    if (items == 0 || parts == 0)
    {
        failures.push_back("no glove frame metadata was audited");
    }

    if (!failures.empty())
    {
        for (const std::string& failure : failures) std::cerr << failure << '\n';
        return 1;
    }
    std::cout << "glove layering audit passed: " << items << " items, "
              << parts << " parts, " << declared.size() << " z labels\n";
    return 0;
}
