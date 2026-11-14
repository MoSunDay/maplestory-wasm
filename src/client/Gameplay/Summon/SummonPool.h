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
#pragma once
#include "../Movement.h"
#include "../MapleMap/MobMovementPlayback.h"
#include "SummonAnims.h"

#include "../Combat/DamageNumber.h"
#include "../../Template/Point.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace jrc
{
    namespace summon
    {
        /// movementType byte of SPAWN_SPECIAL_MAPOBJECT. Unknown values
        /// fall back to STATIONARY, matching the official client.
        constexpr uint8_t STATIONARY     = 0;
        constexpr uint8_t FOLLOW         = 1;
        constexpr uint8_t CIRCLE_FOLLOW  = 3;

        /// Radius of the CIRCLE_FOLLOW orbit around the owner, in map
        /// units, and its angular speed in radians per millisecond.
        constexpr double ORBIT_RADIUS = 50.0;
        constexpr double ORBIT_SPEED  = 6.2831853 / 8000.0;
        /// Fraction of the remaining distance a FOLLOW summon closes per
        /// millisecond when no movement fragments are replaying.
        constexpr double FOLLOW_EASE_PER_MS = 0.004;
        /// Milliseconds the attack animation stays pinned after
        /// SUMMON_ATTACK, and how long a removed summon lingers for its
        /// death animation.
        constexpr uint16_t ATTACK_HOLD_MS = 450;
        constexpr uint16_t DEATH_HOLD_MS  = 800;

        /// Upper bound of stashed events kept per owner cid, so an owner
        /// that never instantiates cannot grow the stash unbounded.
        constexpr size_t MAX_STASHED_SUMMON_EVENTS = 4;

        /// Fields of SPAWN_SPECIAL_MAPOBJECT (and the dragon variant).
        struct SummonSpawn
        {
            int32_t oid = 0;
            int32_t owner_cid = 0;
            int32_t skillid = 0;
            uint8_t skill_level = 0;
            Point<int16_t> position;
            uint8_t stance = 0;
            uint8_t move_type = STATIONARY;
            bool toleft = false;
            bool puppet = false;
            bool animated = false;
        };

        /// One damage line of SUMMON_ATTACK, already decoded.
        struct SummonHit
        {
            int32_t monster_oid = 0;
            int32_t damage = 0;
            bool critical = false;
        };

        /// A remote summon event whose owner is still queued for
        /// instantiation in MapChars; replayed in arrival order once the
        /// owner exists on the map.
        struct SummonEvent
        {
            enum Kind
            {
                SPAWN,  // SPAWN_SPECIAL_MAPOBJECT / SPAWN_DRAGON payload
                MOVE    // MOVE_SUMMON fragments for one summon oid
            };

            Kind kind = SPAWN;
            int32_t oid = 0;                 // identity for same-kind override
            SummonSpawn spawn;               // SPAWN
            Point<int16_t> start;            // MOVE anchor
            std::vector<Movement> movements; // MOVE
        };

        /// One remote summon. The resolved animations decide visibility:
        /// an invalid set renders nothing while the logical position and
        /// lifetime still track the server.
        struct SummonEntity
        {
            int32_t oid = 0;
            int32_t owner_cid = 0;
            int32_t skillid = 0;
            uint8_t skill_level = 0;
            uint8_t move_type = STATIONARY;
            uint8_t stance = 0;
            bool toleft = false;
            bool puppet = false;
            bool animated = false;
            bool dying = false;

            /// Position of this and the previous frame, for draw-time
            /// interpolation like OtherChar's phobj.
            Point<int16_t> position;
            Point<int16_t> previous;

            double orbit_angle = 0.0;
            uint16_t attack_hold = 0;
            uint16_t death_hold = 0;

            Animations anims;
            mobplayback::State playback;
        };

        /// Owner and mob lookups are injected by the caller so the pool
        /// stays decoupled from Stage and MapMobs.
        using OwnerPosition =
            std::function<std::optional<Point<int16_t>>(int32_t cid)>;
        using MobPosition =
            std::function<std::optional<Point<int16_t>>(int32_t mob_oid)>;

        /// All remote summons on the map plus the damage numbers they
        /// produced. Owned by Stage; every operation is a free function.
        struct SummonPool
        {
            std::unordered_map<int32_t, SummonEntity> entities;
            std::vector<DamageNumber> numbers;

            /// SPAWN/MOVE events parked until their owner leaves the
            /// MapChars spawn queue, keyed by owner cid.
            std::map<int32_t, std::vector<SummonEvent>> pending_events;
        };

        /// Spawn a summon from a parsed SPAWN_SPECIAL_MAPOBJECT (or
        /// SPAWN_DRAGON) payload. Replaces an existing entity with the
        /// same object id.
        void spawn(SummonPool& pool, const SummonSpawn& data);

        /// Remove a summon; 'animated' keeps it around briefly so the
        /// death animation can play. Returns true if it existed.
        bool remove(SummonPool& pool, int32_t oid, bool animated);

        /// Replace the movement replay of a summon with a fresh batch of
        /// MOVE_SUMMON fragments anchored at the server start position.
        void move(SummonPool& pool, int32_t oid, Point<int16_t> start,
            std::vector<Movement>&& fragments);

        /// Play the attack direction and place damage numbers on the
        /// struck mobs; unknown mobs drop their line.
        void attack(SummonPool& pool, int32_t oid, bool toleft,
            const std::vector<SummonHit>& hits,
            const MobPosition& mob_position);

        /// Show damage dealt to a summon (DAMAGE_SUMMON).
        void show_damage(SummonPool& pool, int32_t oid, int32_t damage);

        /// Advance every summon by 'dt' milliseconds: replay movement
        /// fragments, then ease FOLLOW summons toward their owner and
        /// orbit CIRCLE_FOLLOW summons around them. STATIONARY summons
        /// hold their spawn position.
        void update(SummonPool& pool, double dt,
            const OwnerPosition& owner_position);

        /// Draw all summons and their damage numbers at 'alpha' render
        /// interpolation, mirroring OtherChar's camera usage.
        void draw(const SummonPool& pool, double viewx, double viewy,
            float alpha);

        /// Drop every summon, e.g. on map change.
        void clear(SummonPool& pool);

        /// Cache an event for an owner that is still queued in MapChars; a
        /// newer event of the same kind for the same oid replaces the
        /// stashed one (both carry idempotent state), and each owner keeps
        /// at most MAX_STASHED_SUMMON_EVENTS events (oldest dropped).
        void stash_event(SummonPool& pool, int32_t owner_cid,
            SummonEvent&& event);

        /// Apply and drop every stashed event whose owner now exists on the
        /// map (owner_exists reports it); events are replayed in arrival
        /// order, kind by kind: SPAWN instantiates, MOVE replaces the
        /// movement replay.
        void replay_stashed(
            SummonPool& pool,
            const std::function<bool(int32_t)>& owner_exists);

        /// Drop stashed events of one owner, e.g. when the cid leaves
        /// the map.
        void drop_stashed(SummonPool& pool, int32_t owner_cid);

        /// Number of tracked summons, dying ones included.
        size_t size(const SummonPool& pool);

        const SummonEntity* find(const SummonPool& pool, int32_t oid);
        SummonEntity* find(SummonPool& pool, int32_t oid);
    }
}
