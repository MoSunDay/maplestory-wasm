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
#include "SkillHitEffect.h"

#include "../../Util/Misc.h"

namespace jrc
{
    SingleHitEffect::SingleHitEffect(nl::node src)
        : effect(src["hit"]["0"]) {}

    void SingleHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        effect.apply(target, user.flip);
    }


    TwoHHitEffect::TwoHHitEffect(nl::node src)
        : effects(src["hit"]["0"], src["hit"]["1"]) {}

    void TwoHHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        effects[user.secondweapon].apply(target, user.flip);
    }

    IndexedHitEffect::IndexedHitEffect(nl::node src, attack_effect::HitIndex i)
        : index(i)
    {
        for (auto sub : src["hit"])
        {
            if (sub["0"].data_type() == nl::node::type::bitmap)
                effects.emplace_back(sub);
        }
    }

    void IndexedHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        if (effects.empty())
            return;

        size_t requested = index == attack_effect::HitIndex::HIT
            ? user.hit_index
            : user.target_index;
        effects[attack_effect::bounded_variant(requested, effects.size())].apply(target, user.flip);
    }


    ByLevelHitEffect::ByLevelHitEffect(nl::node src)
    {
        for (auto sub : src["CharLevel"])
        {
            uint16_t level = string_conversion::or_zero<uint16_t>(sub.name());
            effects.emplace(level, sub["hit"]["0"]);
        }
    }

    void ByLevelHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        if (effects.empty())
            return;

        auto iter = effects.begin();
        for (; iter != effects.end() && user.level > iter->first; ++iter) {}
        if (iter != effects.begin())
            iter--;

        iter->second.apply(target, user.flip);
    }


    ByLevelTwoHHitEffect::ByLevelTwoHHitEffect(nl::node src)
    {
        for (auto sub : src["CharLevel"])
        {
            auto level = string_conversion::or_zero<uint16_t>(sub.name());
            effects.emplace(std::piecewise_construct,
                std::forward_as_tuple(level),
                std::forward_as_tuple(sub["hit"]["0"], sub["hit"]["1"])
                );
        }
    }

    void ByLevelTwoHHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        if (effects.empty())
            return;

        auto iter = effects.begin();
        for (; iter != effects.end() && user.level > iter->first; ++iter) {}
        if (iter != effects.begin())
            iter--;

        iter->second[user.secondweapon].apply(target, user.flip);
    }

    ByLevelIndexedHitEffect::ByLevelIndexedHitEffect(
        nl::node src,
        attack_effect::HitIndex i)
        : index(i)
    {
        for (auto level_node : src["CharLevel"])
        {
            uint16_t level = string_conversion::or_zero<uint16_t>(level_node.name());
            std::vector<Effect> variants;
            for (auto hit : level_node["hit"])
            {
                if (hit["0"].data_type() == nl::node::type::bitmap)
                    variants.emplace_back(hit);
            }
            if (!variants.empty())
                effects.emplace(level, std::move(variants));
        }
    }

    void ByLevelIndexedHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        if (effects.empty())
            return;

        auto iter = effects.upper_bound(user.level);
        if (iter != effects.begin())
            --iter;

        const auto& variants = iter->second;
        size_t requested = index == attack_effect::HitIndex::HIT
            ? user.hit_index
            : user.target_index;
        variants[attack_effect::bounded_variant(requested, variants.size())].apply(target, user.flip);
    }

    BySkillLevelHitEffect::BySkillLevelHitEffect(nl::node src)
    {
        for (auto sub : src["level"])
        {
            auto level = string_conversion::or_zero<int32_t>(sub.name());
            effects.emplace(level, sub["hit"]["0"]);
        }
    }

    void BySkillLevelHitEffect::apply(const AttackUser& user, Mob& target) const
    {
        auto iter = effects.find(user.skilllevel);
        if (iter != effects.end())
        {
            iter->second.apply(target, user.flip);
        }
    }
}
