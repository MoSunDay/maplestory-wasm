#include "client/Audio/FieldSoundPath.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}

    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace
{
    bool resolves_field_audio(nl::node root, const std::string& path)
    {
        for (const std::string& candidate : jrc::field_sound::candidate_paths(path))
        {
            if (root.resolve(candidate).data_type() == nl::node::type::audio)
            {
                return true;
            }
        }
        return false;
    }

    size_t named_gate_count(nl::node map)
    {
        size_t count = 0;
        for (nl::node layer : map)
        {
            for (nl::node object : layer["obj"])
            {
                if (object["name"].get_string() == "gate")
                {
                    count++;
                }
            }
        }
        return count;
    }
}

int main(int argc, char** argv)
{
    const std::string map_path = argc > 1 ? argv[1] : "Map.nx";
    const std::string sound_path = argc > 2 ? argv[2] : "Sound.nx";
    nl::file map_file(map_path);
    nl::file sound_file(sound_path);

    bool ok = resolves_field_audio(sound_file.root(), "Party1/Clear") &&
        resolves_field_audio(sound_file.root(), "Party1/Failed");

    for (int32_t map_id = 103000800; map_id <= 103000804; ++map_id)
    {
        const nl::node map = map_file.root()["Map"]["Map1"]
            [std::to_string(map_id) + ".img"];
        ok = ok && named_gate_count(map) == 1;
    }

    if (!ok)
    {
        std::cerr << "FAIL KPQ gate names or Party1 field sounds are missing\n";
        return 1;
    }

    std::cout << "PASS five KPQ gates and Party1 clear/failed field sounds\n";
    return 0;
}
