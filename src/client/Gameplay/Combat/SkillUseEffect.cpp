//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "SkillUseEffect.h"

#include "../../Util/Misc.h"
#include "../../Character/SkillId.h"

#include <algorithm>

namespace jrc
{
    SingleUseEffect::SingleUseEffect(nl::node src)
        : effect(src["effect"]) {}

    void SingleUseEffect::apply(Char& target) const
    {
        effect.apply(target);
    }


    TwoHUseEffect::TwoHUseEffect(nl::node src)
        : effects(src["effect"]["0"], src["effect"]["1"]) {}

    void TwoHUseEffect::apply(Char& target) const
    {
        bool twohanded = target.is_twohanded();
        effects[twohanded].apply(target);
    }


    MultiUseEffect::MultiUseEffect(nl::node src)
    {
        int8_t no = -1;
        nl::node sub = src["effect"];
        while (sub)
        {
            effects.push_back(sub);

            no++;
            sub = src["effect" + std::to_string(no)];
        }
    }

    void MultiUseEffect::apply(Char& target) const
    {
        for (auto& effect : effects)
        {
            effect.apply(target);
        }
    }


    ByLevelUseEffect::ByLevelUseEffect(nl::node src)
    {
        for (auto sub : src["CharLevel"])
        {
            auto level = string_conversion::or_zero<uint16_t>(sub.name());
            effects.emplace(level, sub["effect"]);
        }
    }

    void ByLevelUseEffect::apply(Char& target) const
    {
        if (effects.empty())
            return;

        uint16_t level = target.get_level();
        auto iter = effects.begin();
        for (; iter != effects.end() && level > iter->first; ++iter) {}
        if (iter != effects.begin())
            iter--;

        iter->second.apply(target);
    }

    FinishUseEffect::FinishUseEffect(nl::node src)
    {
        for (auto sub : src["finish"])
        {
            int16_t orbs = string_conversion::or_zero<int16_t>(sub.name());
            if (orbs > 0 && sub["0"].data_type() == nl::node::type::bitmap)
                effects.emplace(orbs, sub);
        }
    }

    void FinishUseEffect::apply(Char& target) const
    {
        if (effects.empty())
            return;

        // The COMBO value includes the inactive center icon; finish nodes are
        // indexed by the number of consumable orbiting orbs.
        int16_t combo_value = target.get_visual_buff_value(Buffstat::COMBO);
        if (combo_value <= 1)
            return;

        int16_t orbs = combo_value - 1;
        auto iter = effects.lower_bound(orbs);
        if (iter == effects.end())
            iter = std::prev(effects.end());
        iter->second.apply(target);
    }

    ChargedBlowUseEffect::ChargedBlowUseEffect(nl::node src)
    {
        for (auto sub : src["effect"])
        {
            int8_t element = string_conversion::or_zero<int8_t>(sub.name());
            if (element > 0 && sub["0"].data_type() == nl::node::type::bitmap)
                effects.emplace(element, sub);
        }
    }

    void ChargedBlowUseEffect::apply(Char& target) const
    {
        int32_t source = target.get_visual_buff_source(Buffstat::WK_CHARGE);
        int8_t element = 0;
        switch (source)
        {
        case SkillId::SWORD_FIRE_CHARGE:
        case SkillId::BW_FIRE_CHARGE:
            element = 1;
            break;
        case SkillId::SWORD_ICE_CHARGE:
        case SkillId::BW_ICE_CHARGE:
            element = 2;
            break;
        case SkillId::SWORD_LIT_CHARGE:
        case SkillId::BW_LIT_CHARGE:
            element = 3;
            break;
        case SkillId::SWORD_HOLY_CHARGE:
        case SkillId::BW_HOLY_CHARGE:
            element = 5;
            break;
        default:
            return;
        }

        auto iter = effects.find(element);
        if (iter != effects.end())
            iter->second.apply(target);
    }

    void IronBodyUseEffect::apply(Char& target) const
    {
        target.show_iron_body();
    }
}
