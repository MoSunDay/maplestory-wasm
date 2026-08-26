#include "Mob.h"

#include "../Movement.h"

#include "../../Constants.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <functional>


namespace jrc
{
    Mob::Mob(int32_t        oid,
             int32_t        mid,
             int8_t         mode,
             uint8_t        stancebyte,
             uint16_t       fh,
             bool           newspawn,
             int8_t         tm,
             Point<int16_t> position)
        : LivingObject(oid, { true, false, false })
    {
        id = mid;
        std::string strid  = string_format::extend_id(mid, 7);
        const nl::node src = nl::nx::mob[strid + ".img"];

        nl::node info = src["info"];

        level       = info["level"];
        watk        = info["PADamage"];
        matk        = info["MADamage"];
        wdef        = info["PDDamage"];
        mdef        = info["MDDamage"];
        accuracy    = info["acc"];
        avoid       = info["eva"];
        knockback   = info["pushed"];
        speed       = info["speed"];
        flyspeed    = info["flySpeed"];
        touchdamage = info["bodyAttack"].get_bool();
        undead      = info["undead"].get_bool();
        noflip      = info["noFlip"].get_bool();
        notattack   = info["notAttack"].get_bool();
        auto resolve_mob_node = [](std::string rawid) -> nl::node {
            auto try_node = [](const std::string& candidate) -> nl::node {
                if (candidate.empty())
                {
                    return {};
                }

                return nl::nx::mob[candidate + ".img"];
            };

            if (rawid.size() > 4 && rawid.substr(rawid.size() - 4) == ".img")
            {
                rawid = rawid.substr(0, rawid.size() - 4);
            }

            nl::node resolved = try_node(rawid);
            if (resolved)
            {
                return resolved;
            }

            size_t first_non_zero = rawid.find_first_not_of('0');
            std::string trimmed = first_non_zero == std::string::npos ? std::string() : rawid.substr(first_non_zero);
            resolved = try_node(trimmed);
            if (resolved)
            {
                return resolved;
            }

            int64_t numeric = 0;
            try
            {
                std::string numeric_source = trimmed.empty() ? rawid : trimmed;
                numeric = std::stoll(numeric_source);
            }
            catch (...)
            {
                numeric = 0;
            }

            if (numeric > 0)
            {
                resolved = try_node(std::to_string(numeric));
                if (resolved)
                {
                    return resolved;
                }

                resolved = try_node(string_format::extend_id(static_cast<int32_t>(numeric), 7));
                if (resolved)
                {
                    return resolved;
                }
            }

            return {};
        };

        auto has_mob_animations = [](const nl::node& node) {
            return node["stand"].size() > 0 || node["move"].size() > 0 || node["fly"].size() > 0;
        };

        // Some mob entries only carry combat stats and link to another id for visuals.
        std::string linkid = info["link"].get_string();
        nl::node linksrc = resolve_mob_node(linkid);
        if (!linksrc)
        {
            int64_t linknum = info["link"].get_integer(0);
            if (linknum > 0)
            {
                linksrc = resolve_mob_node(std::to_string(linknum));
            }
        }

        nl::node animsrc = linksrc ? linksrc : src;
        if (!has_mob_animations(animsrc) && has_mob_animations(src))
        {
            animsrc = src;
        }

        nl::node flysrc = animsrc["fly"].size() > 0 ? animsrc["fly"] : src["fly"];
        nl::node standsrc = animsrc["stand"].size() > 0 ? animsrc["stand"] : src["stand"];
        nl::node movesrc = animsrc["move"].size() > 0 ? animsrc["move"] : src["move"];
        nl::node jumpsrc = animsrc["jump"].size() > 0 ? animsrc["jump"] : src["jump"];
        nl::node hitsrc = animsrc["hit1"].size() > 0 ? animsrc["hit1"] : src["hit1"];
        nl::node diesrc = animsrc["die1"].size() > 0 ? animsrc["die1"] : src["die1"];

        canfly = flysrc.size() > 0;
        canjump = jumpsrc.size() > 0;
        canmove = movesrc.size() > 0 || canfly;

        if (canfly)
        {
            animations[STAND] = flysrc;
            animations[MOVE]  = flysrc;
        }
        else
        {
            if (standsrc.size() > 0)
            {
                animations[STAND] = standsrc;
            }
            if (movesrc.size() > 0)
            {
                animations[MOVE] = movesrc;
            }
        }

        if (animations.find(STAND) == animations.end() && animations.find(MOVE) != animations.end())
        {
            animations[STAND] = animations[MOVE];
        }
        if (animations.find(MOVE) == animations.end() && animations.find(STAND) != animations.end())
        {
            animations[MOVE] = animations[STAND];
        }

        if (jumpsrc.size() > 0)
        {
            animations[JUMP] = jumpsrc;
        }
        if (hitsrc.size() > 0)
        {
            animations[HIT] = hitsrc;
        }
        if (diesrc.size() > 0)
        {
            animations[DIE] = diesrc;
        }

        if (animations.find(JUMP) == animations.end() && animations.find(MOVE) != animations.end())
        {
            animations[JUMP] = animations[MOVE];
        }
        if (animations.find(HIT) == animations.end() && animations.find(MOVE) != animations.end())
        {
            animations[HIT] = animations[MOVE];
        }
        if (animations.find(DIE) == animations.end() && animations.find(MOVE) != animations.end())
        {
            animations[DIE] = animations[MOVE];
        }

        load_combat(src, animsrc);

        name = nl::nx::string["Mob.img"][std::to_string(mid)]["name"].get_string();

        nl::node sndsrc = nl::nx::sound["Mob.img"][strid];

        hitsound = sndsrc["Damage"];
        diesound = sndsrc["Die"];

        speed += 100;
        speed *= 0.001f;

        flyspeed += 100;
        flyspeed *= 0.0005f;

        if (canfly)
        {
            phobj.type = PhysicsObject::FLYING;
        }

        team = tm;
        set_position(position);
        control = mode > 0;
        aggro = mode == 2;
        phobj.fhid = fh;
        phobj.set_flag(PhysicsObject::TURNATEDGES);

        hppercent    = 0;
        dying        = false;
        dead         = false;
        fading       = false;
        awaitdeath   = false;
        this->stance = STAND;
        set_stance(stancebyte);
        flydirection = STRAIGHT;
        counter      = 0;
        moveid       = 0;
        suggested_skill_id = 0;
        suggested_skill_level = 0;
        next_attack = 0;
        active_action_pos = 0;
        action_elapsed = 0;
        action_hit_delay = 0;
        action_hit_pending = false;
        action_kind = ActionKind::NONE;
        status_first_mask = 0;
        status_second_mask = 0;
        aggro_target = position;

        namelabel = { Text::A13M, Text::CENTER, Text::WHITE, Text::NAMETAG, name };

        if (newspawn)
        {
            fadein = true;
            opacity.set(0.0f);
        }
        else
        {
            fadein = false;
            opacity.set(1.0f);
        }

        if (control && stance == Stance::STAND)
        {
            next_move();
        }
    }

    void Mob::set_stance(uint8_t stancebyte)
    {
        flip = (stancebyte % 2) == 0;
        if (!flip && stancebyte > 0)
        {
            stancebyte -= 1;
        }

        if (stancebyte < MOVE || stancebyte > DIE || (stancebyte % 2) != 0)
        {
            stancebyte = MOVE;
        }

        set_stance(static_cast<Stance>(stancebyte));
    }

    void Mob::set_stance(Stance newstance)
    {
        auto anim = animations.find(newstance);
        if (anim == animations.end())
        {
            newstance = MOVE;
            anim = animations.find(newstance);
            if (anim == animations.end())
            {
                return;
            }
        }

        if (stance != newstance)
        {
            stance = newstance;

            anim->second.reset();
        }
    }

    int8_t Mob::update(const Physics& physics)
    {
        if (!active)
        {
            return phobj.fhlayer;
        }

        for (uint16_t& cooldown : attack_cooldowns)
        {
            cooldown = cooldown > Constants::TIMESTEP ?
                static_cast<uint16_t>(cooldown - Constants::TIMESTEP) : 0;
        }
        if (action_kind != ActionKind::NONE || action_hit_pending)
        {
            action_elapsed = static_cast<uint16_t>(
                std::min<uint32_t>(UINT16_MAX, action_elapsed + Constants::TIMESTEP));
        }

        bool aniend = current_animation().update();
        if (aniend && stance == DIE)
        {
            dead = true;
        }

        if (fading)
        {
            opacity -= 0.025f;
            if (opacity.last() < 0.025f)
            {
                opacity.set(0.0f);
                fading = false;
                dead = true;
            }
        }
        else if (fadein)
        {
            opacity += 0.025f;
            if (opacity.last() > 0.975f)
            {
                opacity.set(1.0f);
                fadein = false;
            }
        }

        if (dead)
        {
            active = false;
            return -1;
        }

        effects.update();
        showhp.update();

        if (!dying)
        {
            if (!canfly)
            {
                if (phobj.is_flag_not_set(PhysicsObject::TURNATEDGES))
                {
                    flip = !flip;
                    phobj.set_flag(PhysicsObject::TURNATEDGES);

                    if (stance == HIT)
                    {
                        set_stance(STAND);
                    }
                }
            }

            if (control && aggro && action_kind == ActionKind::NONE
                && !action_hit_pending)
            {
                update_aggro_action();
            }

            if (action_kind != ActionKind::NONE)
            {
                phobj.hforce = 0.0;
                if (canfly)
                {
                    phobj.vforce = 0.0;
                }
            }
            else switch (stance)
            {
            case MOVE:
                if (canfly)
                {
                    const float status_speed = std::clamp(
                        1.0f + status_value(0x40) / 100.0f, 0.1f, 2.0f);
                    phobj.hforce = (flip ? flyspeed : -flyspeed) * status_speed;
                    switch (flydirection)
                    {
                    case UPWARDS:
                        phobj.vforce = -flyspeed;
                        break;
                    case DOWNWARDS:
                        phobj.vforce = flyspeed;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    const float status_speed = std::clamp(
                        1.0f + status_value(0x40) / 100.0f, 0.1f, 2.0f);
                    phobj.hforce = (flip ? speed : -speed) * status_speed;
                }
                break;
            case HIT:
                if (canmove)
                {
                    double KBFORCE = phobj.onground ? 0.2 : 0.1;
                    phobj.hforce = flip ? -KBFORCE : KBFORCE;
                }
                break;
            case JUMP:
                phobj.vforce = -5.0;
                break;
            default:
                break;
            }

            physics.move_object(phobj);

            if (action_kind != ActionKind::NONE)
            {
                if (aniend)
                {
                    finish_action();
                    if (control)
                    {
                        update_movement();
                    }
                    counter = 0;
                }
                return phobj.fhlayer;
            }

            if (control)
            {
                counter++;

                if (aggro)
                {
                    if (counter >= 30)
                    {
                        update_movement();
                        counter = 0;
                    }
                    return phobj.fhlayer;
                }

                bool next;
                switch (stance)
                {
                case HIT:
                    next = counter > 200;
                    break;
                case JUMP:
                    next = phobj.onground;
                    break;
                default:
                    next = aniend && counter > 200;
                    break;
                }

                if (next)
                {
                    next_move();
                    update_movement();
                    counter = 0;
                }
            }
        }
        else
        {
            phobj.normalize();
            physics.get_fht().update_fh(phobj);
        }

        return phobj.fhlayer;
    }

    void Mob::next_move()
    {
        if (canmove)
        {
            switch (stance)
            {
            case HIT:
            case STAND:
                set_stance(MOVE);
                flip = randomizer.next_bool();
                break;
            case MOVE:
            case JUMP:
                if (canjump && phobj.onground && randomizer.below(0.25f))
                {
                    set_stance(JUMP);
                }
                else
                {
                    switch (randomizer.next_int(3))
                    {
                    case 0:
                        set_stance(STAND);
                        break;
                    case 1:
                        set_stance(MOVE);
                        flip = false;
                        break;
                    case 2:
                        set_stance(MOVE);
                        flip = true;
                        break;
                    default:
                        break;
                    }
                }
                break;
            default:
                break;
            }

            if (stance == MOVE && canfly)
            {
                flydirection = randomizer.next_enum(NUM_DIRECTIONS);
            }
        }
        else
        {
            set_stance(STAND);
        }
    }

    float Mob::calculate_hitchance(int16_t leveldelta, int32_t player_accuracy) const
    {
        auto faccuracy = static_cast<float>(player_accuracy);
        const int32_t effective_avoid = std::max<int32_t>(0, avoid + status_value(0x20));
        float hitchance = faccuracy / (((1.84f + 0.07f * leveldelta) * effective_avoid) + 1.0f);
        if (hitchance < 0.01f)
        {
            hitchance = 0.01f;
        }

        return hitchance;
    }

    double Mob::calculate_mindamage(int16_t leveldelta, double damage, bool magic) const
    {
        const int32_t defense = magic
            ? std::max<int32_t>(0, mdef + status_value(0x8) + status_value(0x8000))
            : std::max<int32_t>(0, wdef + status_value(0x2) + status_value(0x2000));
        double mindamage = magic ?
            damage - (1 + 0.01 * leveldelta) * defense * 0.6 :
            damage * (1 - 0.01 * leveldelta) - defense * 0.6;

        return mindamage < 1.0 ? 1.0 : mindamage;
    }

    double Mob::calculate_maxdamage(int16_t leveldelta, double damage, bool magic) const
    {
        const int32_t defense = magic
            ? std::max<int32_t>(0, mdef + status_value(0x8) + status_value(0x8000))
            : std::max<int32_t>(0, wdef + status_value(0x2) + status_value(0x2000));
        double maxdamage = magic ?
            damage - (1 + 0.01 * leveldelta) * defense * 0.5 :
            damage * (1 - 0.01 * leveldelta) - defense * 0.5;

        return maxdamage < 1.0 ? 1.0 : maxdamage;
    }

    std::vector<std::pair<int32_t, bool>> Mob::calculate_damage(const Attack& attack)
    {
        const bool magic_attack = attack.damagetype == Attack::DMG_MAGIC;
        const uint32_t immunity = magic_attack ? 0x80000 : 0x40000;
        if ((status_second_mask & immunity) != 0)
        {
            return std::vector<std::pair<int32_t, bool>>(attack.hitcount, { 0, false });
        }

        double mindamage;
        double maxdamage;
        float hitchance;
        float critical;
        int16_t leveldelta = level - attack.playerlevel;
        if (leveldelta < 0)
        {
            leveldelta = 0;
        }

        Attack::DamageType damagetype = attack.damagetype;
        switch (damagetype)
        {
        case Attack::DMG_WEAPON:
        case Attack::DMG_MAGIC:
            mindamage = calculate_mindamage(leveldelta, attack.mindamage, damagetype == Attack::DMG_MAGIC);
            maxdamage = calculate_maxdamage(leveldelta, attack.maxdamage, damagetype == Attack::DMG_MAGIC);
            hitchance = calculate_hitchance(leveldelta, attack.accuracy);
            critical  = attack.critical;
            break;
        case Attack::DMG_FIXED:
            mindamage = attack.fixdamage;
            maxdamage = attack.fixdamage;
            hitchance = 1.0f;
            critical  = 0.0f;
            break;
        }

        std::vector<std::pair<int32_t, bool>> result(attack.hitcount);
        std::generate(result.begin(), result.end(), [&](){
            return next_damage(mindamage, maxdamage, hitchance, critical);
        });

        update_movement();
        awaitdeath = false;

        return result;
    }

    std::pair<int32_t, bool> Mob::next_damage(double mindamage, double maxdamage, float hitchance, float critical) const
    {
        bool hit = randomizer.below(hitchance);
        if (!hit)
        {
            return { 0, false };
        }

        constexpr double DAMAGECAP = 999999.0;

        double damage = randomizer.next_real(mindamage, maxdamage);
        bool iscritical = randomizer.below(critical);
        if ((status_second_mask & 0x200000) != 0 && !iscritical)
        {
            return { 0, false };
        }
        if (iscritical)
        {
            damage *= 1.5;
        }

        if (damage < 1)
        {
            damage = 1;
        }
        else if (damage > DAMAGECAP)
        {
            damage = DAMAGECAP;
        }

        auto intdamage = static_cast<int32_t>(damage);
        return { intdamage, iscritical };
    }

    void Mob::apply_damage(int32_t damage, bool toleft)
    {
        hitsound.play();

        if (dying && stance != DIE)
        {
            apply_death();
        }
        else if (control && is_alive() && damage >= knockback)
        {
            flip = toleft;
            counter = 170;
            set_stance(HIT);

            update_movement();
            awaitdeath = true;
        }
    }

    MobAttack Mob::create_touch_attack() const
    {
        if (!touchdamage)
        {
            return {};
        }

        const int32_t effective_watk = std::max<int32_t>(0,
            watk + status_value(0x1) + status_value(0x1000));
        auto minattack = static_cast<int32_t>(effective_watk * 0.8f);
        int32_t maxattack = effective_watk;
        int32_t attack = randomizer.next_int(minattack, maxattack);
        return { attack, get_position(), id, oid };
    }

    void Mob::apply_death()
    {
        cancel_action();
        set_stance(DIE);
        diesound.play();
        dying = true;
    }

    bool Mob::is_alive() const
    {
        return active && !dying;
    }

    bool Mob::is_in_range(const Rectangle<int16_t>& range) const
    {
        if (!active)
        {
            return false;
        }

        Rectangle<int16_t> bounds = animations.at(stance).get_bounds();
        bounds.shift(get_position());
        return range.overlaps(bounds);
    }

}
