#pragma once

#include "../Stance.h"

#include <array>
#include <vector>

namespace jrc::regular_attack
{
    inline const std::vector<Stance::Id>& choices(uint8_t attack, bool degenerate)
    {
        constexpr size_t NUM_ATTACKS = 10;
        static const std::array<std::vector<Stance::Id>, NUM_ATTACKS> normal = { {
            {},
            { Stance::STABO1, Stance::STABO2, Stance::SWINGO1, Stance::SWINGO2, Stance::SWINGO3 },
            { Stance::STABT1, Stance::SWINGP1 },
            { Stance::SHOOT1 },
            { Stance::SHOOT2 },
            { Stance::STABO1, Stance::STABO2, Stance::SWINGT1, Stance::SWINGT2, Stance::SWINGT3 },
            { Stance::SWINGO1, Stance::SWINGO2 },
            { Stance::SWINGO1, Stance::SWINGO2 },
            {},
            { Stance::SHOT }
        } };
        static const std::array<std::vector<Stance::Id>, NUM_ATTACKS> degenerate_choices = { {
            {}, {}, {},
            { Stance::SWINGT1, Stance::SWINGT3 },
            { Stance::SWINGT1, Stance::STABT1 },
            {}, {},
            { Stance::SWINGT1, Stance::STABT1 },
            {},
            { Stance::SWINGP1, Stance::STABT2 }
        } };
        static const std::vector<Stance::Id> none;

        if (attack == 0 || attack >= NUM_ATTACKS)
        {
            return none;
        }
        return degenerate ? degenerate_choices[attack] : normal[attack];
    }

    inline std::vector<Stance::Id> all_choices(uint8_t attack)
    {
        std::vector<Stance::Id> result = choices(attack, false);
        for (Stance::Id stance : choices(attack, true))
        {
            bool duplicate = false;
            for (Stance::Id existing : result)
            {
                duplicate = duplicate || existing == stance;
            }
            if (!duplicate)
            {
                result.push_back(stance);
            }
        }
        result.push_back(Stance::PRONESTAB);
        return result;
    }
}
