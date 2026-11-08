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
        const bool was_controlled = control;
        control = mode > 0;
        aggro = mode == 2;
        if (!control)
        {
            suggested_skill_id = 0;
            suggested_skill_level = 0;
            cancel_action();
            // Drop any stale fragment batch so the mob freezes where the
            // server last saw it until the next MOB_MOVED arrives.
            playback = mobplayback::begin(get_position(), {});
        }
        else if (!was_controlled)
        {
            // A fresh controller must not keep replaying another client's
            // movement batch; local physics takes over from the last
            // server-authoritative position.
            cancel_action();
            playback = mobplayback::begin(get_position(), {});
        }

        last_net_position = get_position();
        last_net_elapsed = net_elapsed;
    }

    void Mob::respawn(Point<int16_t> position, uint8_t stancebyte, uint16_t fh)
    {
        set_position(position);
        phobj.fhid = fh;
        set_stance(stancebyte);
        playback = mobplayback::begin(position, {});
        last_net_position = position;
        last_net_elapsed = net_elapsed;

        // A server respawn for an existing oid must fully revive the
        // instance; leftover death/fade state would keep it untargetable.
        dying = false;
        dead = false;
        fading = false;
        hppercent = 0;
    }

    void Mob::send_movement(Point<int16_t> start, uint8_t raw_activity,
                            uint8_t skill_id, uint8_t skill_level,
                            std::vector<Movement>&& incoming)
    {
        if (control)
        {
            return;
        }
        // The server subtracts two pixels from the reported start position
        // before broadcasting MOVE_MONSTER; undo that so observer and
        // controller coordinates stay identical.
        start = { start.x(), static_cast<int16_t>(start.y() + 2) };

        // Server-authoritative re-anchor: replay restarts from 'start' and
        // fragment positions are interpolated from there, so the mob no
        // longer gets snapped to the batch's final position on arrival.
        set_position(start);
        playback = mobplayback::begin(start, std::move(incoming));
        if (!playback.started())
        {
            return;
        }

        // Attacks and skills trigger on packet arrival: their timing comes
        // from the attack/skill animations, not from the movement path.
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

        // Seed the foothold until the first playback frame applies it.
        if (const Movement* last = mobplayback::last_positioned(playback))
        {
            phobj.fhid = last->fh;
        }
    }

    int8_t Mob::update_remote(const Physics& physics, bool aniend)
    {
        const mobplayback::Sample sample =
            mobplayback::step(playback, Constants::TIMESTEP);

        // Nothing may move a non-controlled mob except fragment replay.
        phobj.hforce = 0.0;
        phobj.vforce = 0.0;
        phobj.hspeed = 0.0;
        phobj.vspeed = 0.0;

        if (playback.started())
        {
            phobj.x = sample.position.x();
            phobj.y = sample.position.y();
            if (sample.fh > 0)
            {
                phobj.fhid = sample.fh;
            }
            if (sample.state > 0 && sample.state < mobplayback::ACTIVITY_FIRST)
            {
                set_stance(sample.state);
            }
        }

        // Keep derived foothold state (layer, onground) usable for draw
        // order and combat range checks while the mob is remote.
        physics.get_fht().update_fh(phobj);

        if (action_kind != ActionKind::NONE && aniend)
        {
            finish_action();
            counter = 0;
        }

        return phobj.fhlayer;
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
            // Observers never receive the controller's local knockback
            // state, so animation 1 must always advance into the death
            // animation. Otherwise dying is set without dead ever being
            // reached, leaving a ghost mob on the map.
            apply_death();
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
