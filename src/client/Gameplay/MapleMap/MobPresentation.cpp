#include "Mob.h"

#include "MobStatus.h"

#include <algorithm>
#include <utility>

namespace jrc
{
    void Mob::draw(double viewx, double viewy, float alpha) const
    {
        Point<int16_t> position = phobj.get_absolute(viewx, viewy, alpha);
        Point<int16_t> head = get_head_position(position);
        effects.drawbelow(position, alpha);
        if (!dead)
        {
            const float interpolated_opacity = opacity.get(alpha);
            const Color status_color = mob_status::tint(get_status_first_mask());
            const Color draw_color{
                status_color.r(),
                status_color.g(),
                status_color.b(),
                interpolated_opacity
            };
            current_animation().draw(
                DrawArgument(
                    position,
                    position,
                    Point<int16_t>{},
                    flip && !noflip ? -1.0f : 1.0f,
                    1.0f,
                    draw_color,
                    0.0f
                ),
                alpha);
            if (showhp)
            {
                namelabel.draw(position);
                if (!dying && hppercent > 0)
                {
                    hpbar.draw(head, hppercent);
                }
            }
        }
        effects.drawabove(position, alpha);
    }

    void Mob::set_control(int8_t mode)
    {
        control = mode > 0;
        aggro = mode == 2;
        if (!control)
        {
            suggested_skill_id = 0;
            suggested_skill_level = 0;
            cancel_action();
        }
    }

    void Mob::send_movement(Point<int16_t> start, uint8_t raw_activity,
                            uint8_t skill_id, uint8_t skill_level,
                            std::vector<Movement>&& incoming)
    {
        if (control)
        {
            return;
        }
        set_position(start);
        movements = std::move(incoming);
        if (movements.empty())
        {
            return;
        }

        const Movement& last = movements.back();
        if (raw_activity >= 24 && raw_activity <= 41)
        {
            const uint8_t attack_index = (raw_activity - 24) / 2;
            auto attack = std::find_if(normal_attacks.begin(), normal_attacks.end(),
                [attack_index](const NormalAttack& candidate) {
                    return candidate.index == attack_index;
                });
            if (attack != normal_attacks.end())
            {
                begin_attack(static_cast<size_t>(attack - normal_attacks.begin()), false);
            }
        }
        else if (raw_activity >= 42 && raw_activity <= 59)
        {
            auto skill = std::find_if(skill_actions.begin(), skill_actions.end(),
                [skill_id, skill_level](const SkillAction& candidate) {
                    return candidate.id == skill_id && candidate.level == skill_level;
                });
            if (skill != skill_actions.end())
            {
                begin_skill(static_cast<size_t>(skill - skill_actions.begin()), false);
            }
        }
        else
        {
            set_stance(last.newstate);
        }
        phobj.fhid = last.fh;
    }

    Rectangle<int16_t> Mob::get_bounds(Point<int16_t> position) const
    {
        Rectangle<int16_t> bounds = current_animation().get_bounds();
        if (flip && !noflip)
        {
            bounds = {
                static_cast<int16_t>(-bounds.r()),
                static_cast<int16_t>(-bounds.l()),
                bounds.t(), bounds.b()
            };
        }
        bounds.shift(position);
        return bounds;
    }

    Point<int16_t> Mob::get_body_position(Point<int16_t> position) const
    {
        Rectangle<int16_t> bounds = get_bounds(position);
        if (bounds.straight())
        {
            return get_head_position(position);
        }
        const int32_t center_x = static_cast<int32_t>(bounds.l()) + bounds.r();
        const int32_t center_y = static_cast<int32_t>(bounds.t()) + bounds.b();
        return {
            static_cast<int16_t>(center_x / 2),
            static_cast<int16_t>(center_y / 2)
        };
    }

    Point<int16_t> Mob::get_head_position(Point<int16_t> position) const
    {
        Point<int16_t> head = current_animation().get_head();
        position.shift_x((flip && !noflip) ? -head.x() : head.x());
        position.shift_y(head.y());
        return position;
    }

    void Mob::kill(int8_t animation)
    {
        cancel_action();
        switch (animation)
        {
        case 0:
            active = false;
            break;
        case 1:
            dying = true;
            if (awaitdeath)
            {
                apply_death();
            }
            break;
        case 2:
            fading = true;
            dying = true;
            break;
        default:
            break;
        }
    }

    void Mob::show_hp(int8_t percent, uint16_t playerlevel)
    {
        if (hppercent == 0)
        {
            const int16_t delta = playerlevel - level;
            if (delta > 9)
            {
                namelabel.change_color(Text::YELLOW);
            }
            else if (delta < -9)
            {
                namelabel.change_color(Text::RED);
            }
        }
        hppercent = std::clamp<int8_t>(percent, 0, 100);
        showhp.set_for(2000);
    }

    void Mob::show_effect(const Animation& animation, int8_t position,
                          int8_t z, bool effect_flip)
    {
        if (!active)
        {
            return;
        }
        Point<int16_t> shift;
        if (position == 0)
        {
            shift = get_body_position({});
        }
        effects.add(animation, { shift, effect_flip }, z);
    }

    Point<int16_t> Mob::get_body_position() const
    {
        return get_body_position(get_position());
    }

    Point<int16_t> Mob::get_head_position() const
    {
        return get_head_position(get_position());
    }
}
