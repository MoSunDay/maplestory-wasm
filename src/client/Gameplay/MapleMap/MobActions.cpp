#include "Mob.h"

#include "../../Constants.h"
#include "../../Net/Packets/GameplayPackets.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace jrc
{
    namespace
    {
        mob_action::Range read_range(const nl::node& source)
        {
            mob_action::Range result;
            const nl::node range = source["range"];
            if (range["lt"] && range["rb"])
            {
                result.shape = mob_action::Shape::RECTANGLE;
                result.lt = range["lt"];
                result.rb = range["rb"];
            }
            else if (range["sp"] && range["r"])
            {
                result.shape = mob_action::Shape::CIRCLE;
                result.center = range["sp"];
                result.radius = static_cast<int16_t>(range["r"].get_integer(0));
            }
            return result;
        }

        bool has_animation(const nl::node& source)
        {
            for (const nl::node& frame : source)
                if (frame.data_type() == nl::node::type::bitmap) return true;
            return false;
        }
    }

    void Mob::load_combat(const nl::node& source, const nl::node& visual_source)
    {
        for (uint8_t index = 0; index < 9; ++index)
        {
            const std::string name = "attack" + std::to_string(index + 1);
            nl::node attack_source = source[name];
            nl::node attack_visual = visual_source[name];
            if (!attack_source && !attack_visual)
            {
                continue;
            }
            if (!attack_source)
            {
                attack_source = attack_visual;
            }
            if (!attack_visual)
            {
                attack_visual = attack_source;
            }

            const nl::node info = attack_source["info"];
            const mob_action::Range range = read_range(info);
            if (!has_animation(attack_visual) || range.shape == mob_action::Shape::NONE)
            {
                std::cerr << "Mob " << id << " has incomplete " << name
                          << " visual/range data; action disabled" << std::endl;
                continue;
            }

            NormalAttack attack;
            attack.index = index;
            attack.magic = info["magic"].get_bool();
            attack.effect_after = static_cast<uint16_t>(
                std::max<int64_t>(0, info["effectAfter"].get_integer(0)));
            attack.cooldown = static_cast<uint16_t>(
                std::max<int64_t>(0, info["attackAfter"].get_integer(0)));
            attack.range = range;
            attack.animation = Animation(attack_visual);
            normal_attacks.push_back(std::move(attack));
        }
        attack_cooldowns.assign(normal_attacks.size(), 0);

        nl::node bindings = source["info"]["skill"];
        if (!bindings)
        {
            bindings = visual_source["info"]["skill"];
        }
        for (const nl::node& binding : bindings)
        {
            const uint8_t skill_id = static_cast<uint8_t>(binding["skill"].get_integer(0));
            const uint8_t skill_level = static_cast<uint8_t>(binding["level"].get_integer(0));
            const uint8_t action = static_cast<uint8_t>(binding["action"].get_integer(0));
            if (skill_id == 0 || skill_level == 0 || action == 0)
            {
                std::cerr << "Mob " << id << " has an invalid skill binding" << std::endl;
                continue;
            }

            const std::string action_name = "skill" + std::to_string(action);
            nl::node action_visual = visual_source[action_name];
            if (!action_visual)
            {
                action_visual = source[action_name];
            }
            if (!has_animation(action_visual))
            {
                std::cerr << "Mob " << id << " skill " << static_cast<int>(skill_id)
                          << " declares missing " << action_name << std::endl;
                continue;
            }

            const nl::node global = nl::nx::skill["MobSkill.img"]
                [std::to_string(skill_id)]["level"][std::to_string(skill_level)];
            SkillAction skill;
            skill.id = skill_id;
            skill.level = skill_level;
            skill.action = action;
            skill.effect_after = static_cast<uint16_t>(
                std::max<int64_t>(0, binding["effectAfter"].get_integer(0)));
            skill.range = read_range(global);
            skill.animation = Animation(action_visual);
            skill_actions.push_back(std::move(skill));
        }
    }

    Animation& Mob::current_animation()
    {
        if (action_kind == ActionKind::ATTACK && active_action_pos < normal_attacks.size())
        {
            return normal_attacks[active_action_pos].animation;
        }
        if (action_kind == ActionKind::SKILL && active_action_pos < skill_actions.size())
        {
            return skill_actions[active_action_pos].animation;
        }
        return animations.at(stance);
    }

    const Animation& Mob::current_animation() const
    {
        if (action_kind == ActionKind::ATTACK && active_action_pos < normal_attacks.size())
        {
            return normal_attacks[active_action_pos].animation;
        }
        if (action_kind == ActionKind::SKILL && active_action_pos < skill_actions.size())
        {
            return skill_actions[active_action_pos].animation;
        }
        return animations.at(stance);
    }

    void Mob::set_aggro_target(Point<int16_t> target)
    {
        aggro_target = target;
    }

    void Mob::suggest_skill(int16_t response_moveid, bool has_aggro,
                            uint8_t skill_id, uint8_t skill_level)
    {
        if (!control || static_cast<uint16_t>(response_moveid) != moveid)
        {
            return;
        }
        aggro = has_aggro;
        suggested_skill_id = skill_id;
        suggested_skill_level = skill_level;
    }

    bool Mob::action_in_range(const mob_action::Range& range) const
    {
        return range.shape == mob_action::Shape::NONE
            || mob_action::contains(range, get_position(), aggro_target, flip);
    }

    int16_t Mob::status_value(uint32_t mask) const
    {
        auto value = status_values.find(mask);
        return value == status_values.end() ? 0 : value->second;
    }

    void Mob::apply_status(uint32_t first_mask, uint32_t second_mask,
                           const std::vector<int16_t>& values)
    {
        status_first_mask |= first_mask;
        status_second_mask |= second_mask;
        size_t value_index = 0;
        for (uint32_t bit = 1; bit != 0 && value_index < values.size(); bit <<= 1)
        {
            if ((first_mask & bit) != 0)
            {
                ++value_index;
            }
        }
        for (uint32_t bit = 1; bit != 0 && value_index < values.size(); bit <<= 1)
        {
            if ((second_mask & bit) != 0)
            {
                status_values[bit] = values[value_index++];
            }
        }
    }

    void Mob::cancel_status(uint32_t first_mask, uint32_t second_mask)
    {
        status_first_mask &= ~first_mask;
        status_second_mask &= ~second_mask;
        for (uint32_t bit = 1; bit != 0; bit <<= 1)
        {
            if ((second_mask & bit) != 0)
            {
                status_values.erase(bit);
            }
        }
    }

    void Mob::show_status_skill(uint16_t skill_id, uint16_t skill_level)
    {
        const nl::node source = nl::nx::skill["MobSkill.img"]
            [std::to_string(skill_id)]["level"][std::to_string(skill_level)]["effect"];
        if (!source || source.size() == 0)
        {
            std::cerr << "MobSkill " << skill_id << '/' << skill_level
                      << " has no effect animation" << std::endl;
            return;
        }
        show_effect(Animation(source), 0, 1, flip);
    }

    void Mob::update_aggro_action()
    {
        const int32_t raw_dx = aggro_target.x() - get_position().x();
        const int32_t raw_dy = aggro_target.y() - get_position().y();
        const int16_t dx = static_cast<int16_t>(std::clamp<int32_t>(raw_dx, INT16_MIN, INT16_MAX));
        const int16_t dy = static_cast<int16_t>(std::clamp<int32_t>(raw_dy, INT16_MIN, INT16_MAX));
        if (dx != 0)
        {
            flip = dx > 0;
        }

        size_t skill_pos = skill_actions.size();
        if (suggested_skill_id != 0)
        {
            auto skill = std::find_if(skill_actions.begin(), skill_actions.end(), [this](const SkillAction& entry) {
                return entry.id == suggested_skill_id && entry.level == suggested_skill_level;
            });
            skill_pos = static_cast<size_t>(skill - skill_actions.begin());
        }

        size_t attack_pos = normal_attacks.size();
        for (size_t offset = 0; offset < normal_attacks.size(); ++offset)
        {
            const size_t candidate = (next_attack + offset) % normal_attacks.size();
            if (attack_cooldowns[candidate] == 0 && action_in_range(normal_attacks[candidate].range))
            {
                attack_pos = candidate;
                break;
            }
        }

        mob_action::Context context;
        context.aggro = aggro;
        context.can_move = canmove;
        context.can_jump = canjump;
        context.can_fly = canfly;
        context.attack_ready = !notattack && attack_pos < normal_attacks.size();
        context.skill_ready = skill_pos < skill_actions.size();
        context.in_attack_range = context.attack_ready;
        context.in_skill_range = context.skill_ready && action_in_range(skill_actions[skill_pos].range);
        context.on_ground = phobj.onground;
        context.delta_x = dx;
        context.delta_y = dy;

        switch (mob_action::decide(context))
        {
        case mob_action::Decision::SKILL:
            begin_skill(skill_pos, true);
            break;
        case mob_action::Decision::ATTACK:
            begin_attack(attack_pos, true);
            break;
        case mob_action::Decision::JUMP:
            set_stance(JUMP);
            break;
        case mob_action::Decision::CHASE:
            set_stance(MOVE);
            if (canfly)
            {
                flydirection = dy < -20 ? UPWARDS : dy > 20 ? DOWNWARDS : STRAIGHT;
            }
            break;
        default:
            set_stance(STAND);
            break;
        }
    }

    void Mob::begin_attack(size_t attack_pos, bool notify_server)
    {
        if (attack_pos >= normal_attacks.size())
        {
            return;
        }
        active_action_pos = attack_pos;
        action_kind = ActionKind::ATTACK;
        action_elapsed = 0;
        action_hit_delay = normal_attacks[attack_pos].effect_after;
        action_hit_pending = true;
        normal_attacks[attack_pos].animation.reset();
        attack_cooldowns[attack_pos] = normal_attacks[attack_pos].cooldown;
        next_attack = (attack_pos + 1) % normal_attacks.size();
        if (notify_server)
        {
            update_movement();
        }
    }

    void Mob::begin_skill(size_t skill_pos, bool notify_server)
    {
        if (skill_pos >= skill_actions.size())
        {
            return;
        }
        active_action_pos = skill_pos;
        action_kind = ActionKind::SKILL;
        action_elapsed = 0;
        action_hit_pending = false;
        skill_actions[skill_pos].animation.reset();
        suggested_skill_id = 0;
        suggested_skill_level = 0;
        if (notify_server)
        {
            update_movement();
        }
    }

    void Mob::finish_action()
    {
        action_kind = ActionKind::NONE;
        if (!dying)
        {
            set_stance(aggro && canmove ? MOVE : STAND);
        }
    }

    void Mob::cancel_action()
    {
        finish_action();
        action_hit_pending = false;
    }

    MobAttack Mob::poll_action_attack(Point<int16_t> target)
    {
        if (!action_hit_pending || action_elapsed < action_hit_delay
            || active_action_pos >= normal_attacks.size())
        {
            return {};
        }
        action_hit_pending = false;
        const NormalAttack& attack = normal_attacks[active_action_pos];
        if (!mob_action::contains(attack.range, get_position(), target, flip))
        {
            return {};
        }
        return {
            attack.magic ? Attack::MAGIC : Attack::CLOSE,
            std::max<int32_t>(0, watk + status_value(0x1) + status_value(0x1000)),
            std::max<int32_t>(0, matk + status_value(0x4) + status_value(0x4000)),
            get_position(), id, oid, attack.index
        };
    }

    void Mob::update_movement()
    {
        uint8_t raw_activity = value_of(stance, flip);
        uint8_t skill_id = 0;
        uint8_t skill_level = 0;
        if (action_kind == ActionKind::ATTACK && active_action_pos < normal_attacks.size())
        {
            raw_activity = mob_action::normal_activity(normal_attacks[active_action_pos].index, flip);
        }
        else if (action_kind == ActionKind::SKILL && active_action_pos < skill_actions.size())
        {
            const SkillAction& skill = skill_actions[active_action_pos];
            raw_activity = mob_action::skill_activity(skill.action, flip);
            skill_id = skill.id;
            skill_level = skill.level;
        }

        ++moveid;
        MoveMobPacket(
            oid, static_cast<int16_t>(moveid), 0,
            static_cast<int8_t>(raw_activity), static_cast<int8_t>(skill_id),
            static_cast<int8_t>(skill_level), 0, 0,
            get_position(), Movement(phobj, raw_activity)
        ).dispatch();
    }
}
