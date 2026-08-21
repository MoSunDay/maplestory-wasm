#include "FieldSoundPath.h"

namespace jrc::field_sound
{
    std::vector<std::string> candidate_paths(const std::string& server_path)
    {
        if (server_path.empty())
        {
            return {};
        }

        constexpr const char* SOUND_PREFIX = "Sound/";
        constexpr size_t SOUND_PREFIX_LENGTH = 6;
        const std::string normalized =
            server_path.compare(0, SOUND_PREFIX_LENGTH, SOUND_PREFIX) == 0
                ? server_path.substr(SOUND_PREFIX_LENGTH)
                : server_path;

        std::vector<std::string> candidates{ normalized };
        if (normalized.compare(0, 10, "Field.img/") != 0)
        {
            candidates.push_back("Field.img/" + normalized);
        }
        return candidates;
    }
}
