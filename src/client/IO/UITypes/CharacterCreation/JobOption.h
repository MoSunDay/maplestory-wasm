#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jrc::CharacterCreation
{
    // Jobs offered on the creation screen. The enumerator values are the
    // CREATE_CHAR wire codes the linked login server dispatches on
    // (0=Noblesse, 1=Beginner, 2=Legend, 3=Pirate).
    enum class CreationJob : int32_t
    {
        CYGNUS = 0,
        EXPLORER = 1,
        ARAN = 2,
        PIRATE = 3
    };

    inline int32_t wire_code(CreationJob job)
    {
        return static_cast<int32_t>(job);
    }

    inline std::string label(CreationJob job)
    {
        switch (job)
        {
        case CreationJob::CYGNUS:
            return "骑士团";
        case CreationJob::ARAN:
            return "战神";
        case CreationJob::PIRATE:
            return "海盗";
        case CreationJob::EXPLORER:
        default:
            return "冒险家";
        }
    }

    // Starter weapons offered per job. The v83 MakeCharInfo pool only covers
    // the explorer set, so the newer branches pin their own creations:
    // 1442079 polearm (Aran), 1482000 knuckler / 1492000 pistol (Pirate).
    inline std::vector<int32_t> weapon_pool(
        CreationJob job, const std::vector<int32_t>& base)
    {
        switch (job)
        {
        case CreationJob::ARAN:
            return { 1442079 };
        case CreationJob::PIRATE:
            return { 1482000, 1492000 };
        default:
            return base;
        }
    }

    // Creation-screen cycling order: Explorer -> Cygnus -> Aran -> Pirate.
    inline CreationJob cycle(CreationJob job, bool backward = false)
    {
        switch (job)
        {
        case CreationJob::EXPLORER:
            return backward ? CreationJob::PIRATE : CreationJob::CYGNUS;
        case CreationJob::CYGNUS:
            return backward ? CreationJob::EXPLORER : CreationJob::ARAN;
        case CreationJob::ARAN:
            return backward ? CreationJob::CYGNUS : CreationJob::PIRATE;
        case CreationJob::PIRATE:
        default:
            return backward ? CreationJob::ARAN : CreationJob::EXPLORER;
        }
    }
}
