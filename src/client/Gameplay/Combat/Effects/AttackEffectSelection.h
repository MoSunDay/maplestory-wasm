#pragma once

#include "../../../Character/SkillId.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace jrc::attack_effect
{
    enum class HitIndex
    {
        HIT,
        TARGET
    };

    inline std::optional<HitIndex> indexed_hit(
        size_t variants,
        uint8_t attack_count,
        uint8_t mob_count)
    {
        if (variants <= 1)
            return std::nullopt;

        // When both counts match, damage lines are the stable inner ordering;
        // targets are the outer ordering and would otherwise repeat branches.
        if (variants == attack_count)
            return HitIndex::HIT;
        if (variants == mob_count)
            return HitIndex::TARGET;
        return std::nullopt;
    }

    inline size_t bounded_variant(size_t requested, size_t variant_count)
    {
        return variant_count == 0
            ? 0
            : std::min(requested, variant_count - 1);
    }

    inline int16_t consumable_combo_orbs(int16_t combo_value)
    {
        // COMBO includes the inactive center icon.
        return std::max<int16_t>(0, combo_value - 1);
    }

    inline int8_t charged_blow_element(int32_t source)
    {
        switch (source)
        {
        case SkillId::SWORD_FIRE_CHARGE:
        case SkillId::BW_FIRE_CHARGE:
            return 1;
        case SkillId::SWORD_ICE_CHARGE:
        case SkillId::BW_ICE_CHARGE:
            return 2;
        case SkillId::SWORD_LIT_CHARGE:
        case SkillId::BW_LIT_CHARGE:
            return 3;
        case SkillId::SWORD_HOLY_CHARGE:
        case SkillId::BW_HOLY_CHARGE:
            return 5;
        default:
            return 0;
        }
    }
}
