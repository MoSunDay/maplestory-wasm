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
#include "SummonHandlers.h"

#include "../Protocol/SummonPayloads.h"

#include "../../Gameplay/Stage.h"
#include "../../Gameplay/Summon/SummonAnims.h"
#include "../../Gameplay/Summon/SummonPool.h"

#include <optional>

namespace jrc
{
    namespace
    {
        // Owner visibility: present (event applies now), pending (owner is
        // queued in MapChars; SPAWN/MOVE are stashed for replay), or gone
        // (drop, the owner never spawned on this map).
        enum class OwnerState
        {
            PRESENT,
            PENDING,
            GONE
        };

        OwnerState owner_state(int32_t cid)
        {
            Stage& stage = Stage::get();
            if (stage.is_player(cid))
                return OwnerState::PRESENT;

            if (stage.get_chars().get_char(cid))
                return OwnerState::PRESENT;

            if (stage.get_chars().has_pending_spawn(cid))
                return OwnerState::PENDING;

            return OwnerState::GONE;
        }

        std::optional<Point<int16_t>> mob_body_position(int32_t oid)
        {
            MapMobs& mobs = Stage::get().get_mobs();
            if (!mobs.contains(oid))
                return std::nullopt;

            return mobs.get_mob_body_position(oid);
        }

        summon::SummonSpawn to_spawn(
            const summon_packets::SpawnPayload& payload)
        {
            summon::SummonSpawn data;
            data.oid = payload.oid;
            data.owner_cid = payload.owner_cid;
            data.skillid = payload.skillid;
            data.skill_level = payload.skill_level;
            data.position = payload.position;
            data.stance = payload.stance;
            data.move_type = payload.move_type;
            // The wire stance carries no dedicated facing bit for
            // summons; facing is derived from the owner while following.
            data.toleft = payload.stance >= 4;
            data.puppet = payload.puppet;
            data.animated = payload.animated;
            return data;
        }
    }

    void SpawnSummonHandler::handle(InPacket& recv) const
    {
        const summon_packets::SpawnPayload payload =
            summon_packets::parse_spawn(recv);
        switch (owner_state(payload.owner_cid))
        {
            case OwnerState::PRESENT:
                summon::spawn(Stage::get().get_summons(), to_spawn(payload));
                break;
            case OwnerState::PENDING:
            {
                // The owner's SPAWN_PLAYER is still queued in MapChars;
                // replay the summon once the character instantiates.
                summon::SummonEvent event{
                    summon::SummonEvent::SPAWN,
                    payload.oid,
                    to_spawn(payload),
                    {},
                    {}};
                summon::stash_event(
                    Stage::get().get_summons(), payload.owner_cid,
                    std::move(event));
                break;
            }
            case OwnerState::GONE:
                break;
        }
    }

    void RemoveSummonHandler::handle(InPacket& recv) const
    {
        const summon_packets::RemovePayload payload =
            summon_packets::parse_remove(recv);
        summon::remove(
            Stage::get().get_summons(), payload.oid, payload.animated);
    }

    void MoveSummonHandler::handle(InPacket& recv) const
    {
        // Non-const so the fragment list can be moved into the replay.
        summon_packets::MovePayload payload =
            summon_packets::parse_move(recv);
        switch (owner_state(payload.cid))
        {
            case OwnerState::PRESENT:
                summon::move(
                    Stage::get().get_summons(), payload.oid, payload.start,
                    std::move(payload.movements));
                break;
            case OwnerState::PENDING:
            {
                // Movement is idempotent state; stash the newest batch
                // for the summon until its owner instantiates.
                summon::SummonEvent event{
                    summon::SummonEvent::MOVE,
                    payload.oid,
                    {},
                    payload.start,
                    std::move(payload.movements)};
                summon::stash_event(
                    Stage::get().get_summons(), payload.cid,
                    std::move(event));
                break;
            }
            case OwnerState::GONE:
                break;
        }
    }

    void SummonAttackHandler::handle(InPacket& recv) const
    {
        const summon_packets::AttackPayload payload =
            summon_packets::parse_attack(recv);
        // Attacks are transient; they are dropped unless the owner is
        // already visible on the map.
        if (owner_state(payload.cid) != OwnerState::PRESENT)
            return;

        std::vector<summon::SummonHit> hits;
        hits.reserve(payload.hits.size());
        for (const summon_packets::Hit& hit : payload.hits)
            hits.push_back(
                { hit.monster_oid, hit.damage, hit.critical });

        summon::attack(
            Stage::get().get_summons(), payload.summon_oid, payload.toleft,
            hits, mob_body_position);
    }

    void DamageSummonHandler::handle(InPacket& recv) const
    {
        const summon_packets::DamagePayload payload =
            summon_packets::parse_damage(recv);
        // Damage numbers are transient; they are dropped unless the owner
        // is already visible on the map.
        if (owner_state(payload.cid) != OwnerState::PRESENT)
            return;

        summon::show_damage(
            Stage::get().get_summons(), payload.oid, payload.damage);
    }

    void SpawnDragonHandler::handle(InPacket& recv) const
    {
        const summon_packets::DragonPayload payload =
            summon_packets::parse_dragon(recv);

        // Dragons are plain summon entities; the skill animation is
        // derived from the owner's job.
        summon::SummonSpawn data;
        data.oid = payload.owner_cid;
        data.owner_cid = payload.owner_cid;
        data.skillid = summon::dragon_skillid(payload.job);
        data.position = payload.position;
        data.stance = payload.stance;
        data.move_type = summon::FOLLOW;
        data.animated = true;

        switch (owner_state(payload.owner_cid))
        {
            case OwnerState::PRESENT:
                summon::spawn(Stage::get().get_summons(), data);
                break;
            case OwnerState::PENDING:
            {
                summon::SummonEvent event{
                    summon::SummonEvent::SPAWN,
                    payload.owner_cid,
                    data,
                    {},
                    {}};
                summon::stash_event(
                    Stage::get().get_summons(), payload.owner_cid,
                    std::move(event));
                break;
            }
            case OwnerState::GONE:
                break;
        }
    }
}
