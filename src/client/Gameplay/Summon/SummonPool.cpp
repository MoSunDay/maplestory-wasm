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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
// GNU Affero General Public License for more details.                     //
#include "SummonPool.h"

#include "../../Constants.h"
#include "../../Util/Lerp.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace jrc
{
    namespace summon
    {
        namespace
        {
            uint8_t effective_move_type(uint8_t move_type)
            {
                switch (move_type)
                {
                    case FOLLOW:
                    case CIRCLE_FOLLOW:
                        return move_type;
                    default:
                        // Unknown movement types hold still, exactly like
                        // STATIONARY ones.
                        return STATIONARY;
                }
            }

            bool moving(const SummonEntity& entity)
            {
                const int16_t dx = static_cast<int16_t>(
                    std::abs(entity.position.x() - entity.previous.x()));
                const int16_t dy = static_cast<int16_t>(
                    std::abs(entity.position.y() - entity.previous.y()));
                return dx + dy > 0;
            }

            /// Which animation slot the entity is currently showing.
            /// Returning a member pointer keeps one source of truth for
            /// both the mutable update path and the const draw path.
            Animation Animations::* current_anim(
                const SummonEntity& entity)
            {
                if (entity.dying)
                    return &Animations::die;
                if (entity.attack_hold > 0)
                    return &Animations::attack;
                if (moving(entity))
                    return &Animations::move;
                return &Animations::stand;
            }

            /// Fraction of the remaining distance closed over 'dt' ms.
            float ease_factor(double dt)
            {
                return static_cast<float>(
                    std::clamp(FOLLOW_EASE_PER_MS * dt, 0.0, 1.0));
            }

            void ease_toward(SummonEntity& entity, Point<int16_t> target,
                double dt)
            {
                const float alpha = ease_factor(dt);
                entity.toleft = target.x() < entity.position.x();
                entity.position = {
                    lerp(entity.position.x(), target.x(), alpha),
                    lerp(entity.position.y(), target.y(), alpha)
                };
            }

            void orbit_owner(SummonEntity& entity, Point<int16_t> owner,
                double dt)
            {
                entity.orbit_angle += ORBIT_SPEED * dt;
                const double target_x =
                    owner.x() + ORBIT_RADIUS * std::cos(entity.orbit_angle);
                const double target_y =
                    owner.y() - ORBIT_RADIUS * std::sin(entity.orbit_angle);
                ease_toward(
                    entity,
                    {
                        static_cast<int16_t>(std::lround(target_x)),
                        static_cast<int16_t>(std::lround(target_y))
                    },
                    dt);
            }

            void step_autonomous(SummonEntity& entity, double dt,
                const OwnerPosition& owner_position)
            {
                if (!owner_position)
                    return;

                const std::optional<Point<int16_t>> owner =
                    owner_position(entity.owner_cid);
                if (!owner)
                    return;

                switch (effective_move_type(entity.move_type))
                {
                    case FOLLOW:
                        // Follow summons with no fragments pending ease
                        // toward their owner.
                        ease_toward(entity, *owner, dt);
                        break;
                    case CIRCLE_FOLLOW:
                        orbit_owner(entity, *owner, dt);
                        break;
                    default:
                        break;
                }
            }

            void step_animations(SummonEntity& entity, uint16_t timestep)
            {
                (entity.anims.*current_anim(entity)).update(timestep);
                if (entity.attack_hold > timestep)
                    entity.attack_hold -= timestep;
                else
                    entity.attack_hold = 0;
            }

            void place_hit_numbers(SummonPool& pool,
                const std::vector<SummonHit>& hits,
                const MobPosition& mob_position)
            {
                if (!mob_position)
                    return;

                // Stack repeated lines on one mob like Combat's
                // place_numbers, keyed off the mob body position.
                std::unordered_map<int32_t, int16_t> lines;
                for (const SummonHit& hit : hits)
                {
                    if (hit.damage <= 0)
                        continue;

                    const std::optional<Point<int16_t>> body =
                        mob_position(hit.monster_oid);
                    if (!body)
                        continue;

                    int16_t row = lines[hit.monster_oid]++;
                    const int16_t starty = static_cast<int16_t>(
                        body->y() - row * DamageNumber::rowheight(hit.critical));
                    pool.numbers.emplace_back(
                        hit.critical ? DamageNumber::CRITICAL
                                     : DamageNumber::NORMAL,
                        hit.damage, starty);
                    pool.numbers.back().set_x(body->x());
                }
            }
        }

        void spawn(SummonPool& pool, const SummonSpawn& data)
        {
            SummonEntity& entity = pool.entities[data.oid];
            entity.oid = data.oid;
            entity.owner_cid = data.owner_cid;
            entity.skillid = data.skillid;
            entity.skill_level = data.skill_level;
            entity.move_type = effective_move_type(data.move_type);
            entity.stance = data.stance;
            entity.toleft = data.toleft;
            entity.puppet = data.puppet;
            entity.animated = data.animated;
            entity.dying = false;
            entity.position = data.position;
            entity.previous = data.position;
            entity.orbit_angle = 0.0;
            entity.attack_hold = 0;
            entity.death_hold = 0;
            entity.playback = mobplayback::State{};

            entity.anims = summon_animation(data.skillid);
            entity.anims.summoned.reset();
            entity.anims.stand.reset();
        }

        bool remove(SummonPool& pool, int32_t oid, bool animated)
        {
            SummonEntity* entity = find(pool, oid);
            if (!entity)
                return false;

            if (!animated || !entity->anims.valid)
            {
                // Vanishing summons, or ones without data to animate a
                // death with, disappear at once.
                pool.entities.erase(oid);
                return true;
            }

            entity->dying = true;
            entity->death_hold = DEATH_HOLD_MS;
            entity->anims.die.reset();
            return true;
        }

        void move(SummonPool& pool, int32_t oid, Point<int16_t> start,
            std::vector<Movement>&& fragments)
        {
            SummonEntity* entity = find(pool, oid);
            if (!entity)
                return;

            entity->playback =
                mobplayback::begin(start, std::move(fragments));
        }

        void attack(SummonPool& pool, int32_t oid, bool toleft,
            const std::vector<SummonHit>& hits,
            const MobPosition& mob_position)
        {
            SummonEntity* entity = find(pool, oid);
            if (entity)
            {
                entity->toleft = toleft;
                entity->attack_hold = ATTACK_HOLD_MS;
                entity->anims.attack.reset();
            }

            place_hit_numbers(pool, hits, mob_position);
        }

        void show_damage(SummonPool& pool, int32_t oid, int32_t damage)
        {
            const SummonEntity* entity = find(pool, oid);
            if (!entity || damage <= 0)
                return;

            pool.numbers.emplace_back(
                DamageNumber::TOPLAYER, damage, entity->position.y());
            pool.numbers.back().set_x(entity->position.x());
        }

        void update(SummonPool& pool, double dt,
            const OwnerPosition& owner_position)
        {
            const auto timestep = static_cast<uint16_t>(dt);
            for (auto it = pool.entities.begin(); it != pool.entities.end();)
            {
                SummonEntity& entity = it->second;
                entity.previous = entity.position;

                if (entity.dying)
                {
                    if (entity.death_hold > timestep)
                        entity.death_hold -= timestep;
                    else
                        entity.death_hold = 0;
                    entity.anims.die.update(timestep);
                    it = entity.death_hold > 0
                        ? std::next(it)
                        : pool.entities.erase(it);
                    continue;
                }

                if (entity.playback.started())
                {
                    // Fragments own the trajectory while their time
                    // lasts, like remote characters and pets.
                    const mobplayback::Sample sample =
                        mobplayback::step(entity.playback, dt);
                    entity.position = sample.position;
                    entity.stance = sample.state;
                    if (sample.done)
                        entity.playback = mobplayback::State{};
                }
                else
                {
                    step_autonomous(entity, dt, owner_position);
                }

                step_animations(entity, timestep);
                ++it;
            }

            pool.numbers.erase(
                std::remove_if(
                    pool.numbers.begin(), pool.numbers.end(),
                    [](DamageNumber& number) { return !number.update(); }),
                pool.numbers.end());
        }

        void draw(const SummonPool& pool, double viewx, double viewy,
            float alpha)
        {
            for (const auto& entry : pool.entities)
            {
                const SummonEntity& entity = entry.second;
                const Animation& anim = entity.anims.*current_anim(entity);
                // Missing skill data degrades to invisible-but-synced:
                // no frames are drawn, the entity itself is kept.
                if (!anim.is_valid())
                    continue;

                const Point<int16_t> inter = {
                    lerp(entity.previous.x(), entity.position.x(), alpha),
                    lerp(entity.previous.y(), entity.position.y(), alpha)
                };
                const Point<int16_t> absp = {
                    static_cast<int16_t>(std::lround(inter.x() + viewx)),
                    static_cast<int16_t>(std::lround(inter.y() + viewy))
                };
                anim.draw(DrawArgument(absp, entity.toleft), alpha);
            }

            for (const DamageNumber& number : pool.numbers)
                number.draw(viewx, viewy, alpha);
        }

        void clear(SummonPool& pool)
        {
            pool.entities.clear();
            pool.numbers.clear();
            pool.pending_events.clear();
        }

        void stash_event(SummonPool& pool, int32_t owner_cid,
            SummonEvent&& event)
        {
            // SPAWN/MOVE can share a network batch with the owner's
            // SPAWN_PLAYER; keep the newest event per kind and oid for the
            // replay pass instead of dropping it. The per-owner cap bounds
            // an owner that never instantiates.
            std::vector<SummonEvent>& events = pool.pending_events[owner_cid];
            auto stashed = std::find_if(
                events.begin(),
                events.end(),
                [&event](const SummonEvent& entry)
                {
                    return entry.kind == event.kind
                        && entry.oid == event.oid;
                }
            );
            if (stashed != events.end())
                *stashed = std::move(event);
            else
                events.push_back(std::move(event));

            if (events.size() > MAX_STASHED_SUMMON_EVENTS)
                events.erase(events.begin());
        }

        void replay_stashed(
            SummonPool& pool,
            const std::function<bool(int32_t)>& owner_exists)
        {
            // Called right after MapChars::update, so a stash whose owner
            // still cannot be found is stale and simply waits for a later
            // pass or a map change.
            std::vector<int32_t> ready;
            ready.reserve(pool.pending_events.size());
            for (const auto& entry : pool.pending_events)
            {
                if (owner_exists(entry.first))
                    ready.push_back(entry.first);
            }

            for (int32_t owner_cid : ready)
            {
                auto iter = pool.pending_events.find(owner_cid);
                for (SummonEvent& event : iter->second)
                {
                    switch (event.kind)
                    {
                        case SummonEvent::SPAWN:
                            spawn(pool, event.spawn);
                            break;
                        case SummonEvent::MOVE:
                            move(pool, event.oid, event.start,
                                std::move(event.movements));
                            break;
                    }
                }
                pool.pending_events.erase(iter);
            }
        }

        void drop_stashed(SummonPool& pool, int32_t owner_cid)
        {
            pool.pending_events.erase(owner_cid);
        }

        size_t size(const SummonPool& pool)
        {
            return pool.entities.size();
        }

        const SummonEntity* find(const SummonPool& pool, int32_t oid)
        {
            auto iter = pool.entities.find(oid);
            return iter == pool.entities.end() ? nullptr : &iter->second;
        }

        SummonEntity* find(SummonPool& pool, int32_t oid)
        {
            auto iter = pool.entities.find(oid);
            return iter == pool.entities.end() ? nullptr : &iter->second;
        }
    }
}
