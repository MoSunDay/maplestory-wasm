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
#include "MapObjectHandlers.h"

#include "Helpers/LoginParser.h"
#include "Helpers/BuffMaskOrder.h"
#include "Helpers/MovementParser.h"
#include "../Protocol/DropRemovalPayload.h"
#include "../Protocol/SpawnMobPayload.h"
#include "../Protocol/SpawnPlayerPayload.h"

#include "../../Audio/Audio.h"
#include "../../Character/OtherChar.h"
#include "../../Character/SkillId.h"
#include "../../Console.h"
#include "../../Gameplay/Stage.h"
#include "../../Gameplay/Spawn.h"
#include "../../Gameplay/Summon/SummonPool.h"
#include "../../Template/Optional.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace jrc
{
    namespace
    {
        void spawn_mob_from_packet(
            InPacket& recv,
            int32_t oid,
            int32_t id,
            int8_t mode,
            spawn_mob::Layout layout = spawn_mob::Layout::STANDARD)
        {
            const spawn_mob::Payload payload = spawn_mob::parse_payload(recv, layout);

            Stage::get().get_mobs().spawn({
                oid,
                id,
                mode,
                payload.stance,
                payload.foothold,
                payload.new_spawn,
                payload.team,
                payload.position
            });
        }

        // Disease (debuff) bits of the second buff mask: POISON writes a
        // value short before the skill id and skill level shorts pair.
        constexpr uint64_t POISON_FLAG = 0x4000000000000;

        bool is_disease_flag(uint64_t flag)
        {
            switch (flag)
            {
                case 0x1:                // slow
                case 0x80:               // seduce
                case 0x4000:             // zombify
                case 0x80000:            // confuse
                case 0x2000000000000:    // stun
                case POISON_FLAG:        // poison
                case 0x8000000000000:    // seal
                case 0x10000000000000:   // darkness
                case 0x4000000000000000: // weaken
                case 0x8000000000000000: // curse
                    return true;
                default:
                    return false;
            }
        }

        // Skill variants of SHOW_FOREIGN_EFFECT are cid + effect + skillid
        // + direction (+9 more bytes); the tail is not consumed here.
        void show_foreign_skill_effect(InPacket& recv, int32_t cid, int8_t effect)
        {
            if (recv.length() < 5)
                return;

            int32_t skillid = recv.read_int();
            recv.read_byte(); // 'direction'
            // 9 more bytes after this

            if ((skillid >= SkillId::SWORD_FIRE_CHARGE
                    && skillid <= SkillId::BW_LIT_CHARGE)
                || skillid == SkillId::SWORD_HOLY_CHARGE
                || skillid == SkillId::BW_HOLY_CHARGE)
            {
                Stage::get().get_chars().set_visual_buff_source(
                    cid, Buffstat::WK_CHARGE, skillid
                );
            }

            Stage::get().get_combat().show_buff(cid, skillid, effect);
        }
    }

    void SpawnCharHandler::handle(InPacket& recv) const
    {
        const spawn_player::Payload payload = spawn_player::parse_payload(recv);

        CharSpawn charspawn({
            payload.header.cid,
            payload.look,
            payload.header.level,
            payload.job,
            payload.header.name,
            payload.stance,
            payload.position,
            payload.combo_value,
            payload.chair_id,
            payload.chalkboard
        });
        charspawn.set_pets(payload.pets, payload.pet_count);
        Stage::get().get_chars().spawn(std::move(charspawn));
    }


    void RemoveCharHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();

        Stage::get().get_chars().remove(cid);
        // The cid left the map; drop any summon events stashed while its
        // SPAWN_PLAYER was still queued, mirroring MapChars::remove.
        summon::drop_stashed(Stage::get().get_summons(), cid);
    }


    void SpawnPetHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        uint8_t petindex = recv.read_byte();
        int8_t mode = recv.read_byte();

        // Parse the body before the owner resolves; survives a queued spawn.
        MapChars::PetUpdate update{};
        update.index = petindex;
        if (mode == 1)
        {
            recv.skip(1);

            update.despawn = false;
            update.pet.itemid = recv.read_int();
            update.pet.name = recv.read_string();
            update.pet.uniqueid = recv.read_int();

            recv.skip(4);

            update.pet.position = recv.read_point();
            update.pet.stance = recv.read_byte();
            update.pet.foothold = recv.read_int();
        }
        else if (mode == 0)
        {
            update.despawn = true;
            update.hunger = recv.read_bool();
        }
        else
        {
            return;
        }

        if (Optional<Char> character = Stage::get().get_character(cid))
        {
            if (update.despawn)
                character->remove_pet(update.index, update.hunger);
            else
                character->add_pet(update.index, update.pet.itemid,
                    update.pet.name, update.pet.uniqueid, update.pet.position,
                    update.pet.stance, update.pet.foothold);
        }
        else
        {
            Stage::get().get_chars().stash_pet(cid, update);
        }
    }


    void PetMovedHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        uint8_t petindex = recv.read_byte();
        recv.read_int(); // pet unique id
        std::vector<Movement> movements = MovementParser::parse_movements(recv);

        // Parsed before the owner resolves, so it survives a queued spawn.
        Stage::get().get_chars().move_pet(cid, petindex, movements);
    }


    void CharMovedHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        recv.skip(4);
        std::vector<Movement> movements = MovementParser::parse_movements(recv);

        Stage::get().get_chars().send_movement(cid, movements);
    }


    void UpdateCharLookHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        recv.read_byte();
        LookEntry look = LoginParser::parse_look(recv);

        Stage::get().get_chars().update_look(cid, look);
    }

    void ShowChairHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        int32_t item_id = recv.read_int();

        // Routed through MapChars so a queued spawn stashes it, not drops.
        Stage::get().get_chars().set_chair(cid, item_id);
    }

    void FacialExpressionHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        int32_t emote = recv.read_int();

        // Routed through MapChars so a queued spawn stashes it, not drops.
        Stage::get().get_chars().set_expression(cid, emote);
    }

    void ShowForeignEffectHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        int8_t effect = recv.read_byte();

        // The effect id alone decides the payload layout. Probing by
        // remaining length first would misroute bare packets (e.g. the
        // maker result int) into the skill branch, so dispatch on the id.
        switch (effect)
        {
            case 0: // level up
                Stage::get().show_character_effect(cid, CharEffect::LEVELUP);
                Sound(Sound::LEVELUP).play();
                break;
            case 1: // buff effect (skill)
            case 2: // buff effect (skill)
            case 5: // buff effect (skill)
                show_foreign_skill_effect(recv, cid, effect);
                break;
            case 8: // job advance
                Stage::get().show_character_effect(cid, CharEffect::JOBCHANGE);
                break;
            case 9: // quest complete; no payload, no sound asset yet
                break;
            case 10: // recovery
                if (recv.length() >= 1)
                    recv.read_byte(); // 'amount'
                break;
            case 11: // buff (buyback); carries a trailing int
            case 15: // maker / equipment level up; may carry a trailing int
            case 16: // maker result; 0 = success
                if (recv.length() >= 4)
                    recv.read_int();
                break;
            case 13: // card effect
                Stage::get().show_character_effect(cid, CharEffect::MONSTER_CARD);
                if (recv.length() >= 4)
                    recv.read_int(); // skillid
                break;
            default:
                // 6, 7, 14, 17, 19, 21 and 26 carry no payload in v83.
                break;
        }
    }

    void GiveForeignBuffHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        uint64_t firstmask = recv.read_long();
        uint64_t secondmask = recv.read_long();

        struct VisualUpdate
        {
            Buffstat::Id stat;
            int16_t value;
        };
        std::vector<VisualUpdate> updates;
        bool darksight = false;
        bool shadowpartner = false;
        bool morphed = false;

        // Parse the complete packet before mutating character state. A
        // truncated packet must not leave half of a multi-buff update applied.
        const size_t first_count = buff_mask::ordered_flags(firstmask).size();
        for (size_t i = 0; i < first_count; ++i)
            recv.read_short();

        // The second mask mixes regular buffs (one value short) with
        // diseases: POISON adds a value short before the skill pair.
        const auto second_ids = buff_mask::ordered_ids<Buffstat::Id>(
            secondmask, Buffstat::second_codes);
        for (const Buffstat::Id stat : second_ids.ids)
        {
            const uint64_t flag = Buffstat::second_codes.at(stat);
            if (flag == POISON_FLAG)
            {
                const int16_t value = recv.read_short();
                recv.read_short(); // skill id
                recv.read_short(); // skill level
                updates.push_back({ Buffstat::POISON, value });
            }
            else if (is_disease_flag(flag))
            {
                recv.read_short(); // mob skill id
                recv.read_short(); // mob skill level
                updates.push_back({ stat, 1 });
            }
            else
            {
                const int16_t value = recv.read_short();
                if (stat == Buffstat::COMBO || stat == Buffstat::WK_CHARGE)
                    updates.push_back({ stat, value });
                else if (stat == Buffstat::DARKSIGHT)
                    darksight = true;
                else if (stat == Buffstat::SHADOWPARTNER)
                    shadowpartner = true;
                else if (stat == Buffstat::MORPH)
                    morphed = true;
            }
        }

        for (const VisualUpdate& update : updates)
            Stage::get().get_chars().set_visual_buff(cid, update.stat, update.value);

        // Whole-body looks land as dedicated character flags, not buffs.
        if (darksight || shadowpartner || morphed)
        {
            if (Optional<OtherChar> character = Stage::get().get_chars().get_char(cid))
            {
                if (darksight)
                    character->set_darksight(true);
                if (shadowpartner)
                    character->set_shadowpartner(true);
                if (morphed)
                    character->set_morphed(true);
            }
        }
    }

    void CancelForeignBuffHandler::handle(InPacket& recv) const
    {
        int32_t cid = recv.read_int();
        uint64_t firstmask = recv.read_long();
        uint64_t secondmask = recv.read_long();

        // DARKSIGHT, SHADOWPARTNER and MORPH are second-mask stats on the
        // server, but checking both masks keeps clearing the looks robust.
        const uint64_t lookmask = firstmask | secondmask;
        if (lookmask & (Buffstat::second_codes.at(Buffstat::DARKSIGHT)
                | Buffstat::second_codes.at(Buffstat::SHADOWPARTNER)
                | Buffstat::second_codes.at(Buffstat::MORPH)))
        {
            if (Optional<OtherChar> character = Stage::get().get_chars().get_char(cid))
            {
                if (lookmask & Buffstat::second_codes.at(Buffstat::DARKSIGHT))
                    character->set_darksight(false);
                if (lookmask & Buffstat::second_codes.at(Buffstat::SHADOWPARTNER))
                    character->set_shadowpartner(false);
                if (lookmask & Buffstat::second_codes.at(Buffstat::MORPH))
                    character->set_morphed(false);
            }
        }

        if (secondmask & Buffstat::second_codes.at(Buffstat::COMBO))
            Stage::get().get_chars().cancel_visual_buff(cid, Buffstat::COMBO);
        if (secondmask & Buffstat::second_codes.at(Buffstat::WK_CHARGE))
            Stage::get().get_chars().cancel_visual_buff(cid, Buffstat::WK_CHARGE);

        for (const Buffstat::Id stat : {Buffstat::SLOW, Buffstat::SEDUCE, Buffstat::ZOMBIFY, Buffstat::CONFUSE,
             Buffstat::STUN, Buffstat::POISON, Buffstat::SEAL, Buffstat::DARKNESS, Buffstat::WEAKEN, Buffstat::CURSE})
        {
            if (secondmask & Buffstat::second_codes.at(stat))
                Stage::get().get_chars().cancel_visual_buff(cid, stat);
        }
    }


    void SpawnMobHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        recv.read_byte(); // 5 if controller == null
        int32_t id = recv.read_int();
        const spawn_mob::Layout layout = Stage::get().get_mobs().contains(oid)
            ? spawn_mob::Layout::MAKE_REAL
            : spawn_mob::Layout::STANDARD;
        spawn_mob_from_packet(recv, oid, id, 0, layout);
    }


    void KillMobHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        int8_t animation = recv.read_byte();

        Stage::get().get_mobs().remove(oid, animation);
    }


    void SpawnMobControllerHandler::handle(InPacket& recv) const
    {
        int8_t mode = recv.read_byte();
        int32_t oid = recv.read_int();
        if (mode == 0)
        {
            Stage::get().get_mobs().set_control(oid, false);
        }
        else
        {
            if (recv.available())
            {
                recv.skip(1);

                int32_t id = recv.read_int();
                spawn_mob_from_packet(recv, oid, id, mode);
            }
            else
            {
                // remove monster invisibility, not used in moopledev or solaxia (maybe in an event script?)
            }
        }
    }


    void MobMovedHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();

        recv.read_byte(); // reserved
        recv.read_byte(); // can use a skill on the next movement
        uint8_t raw_activity = recv.read_byte();
        uint8_t skill_id = recv.read_byte();
        uint8_t skill_level = recv.read_byte();
        recv.read_short(); // option

        Point<int16_t> position = recv.read_point();
        std::vector<Movement> movements = MovementParser::parse_movements(recv);

        Stage::get().get_mobs().send_movement(
            oid, position, raw_activity, skill_id, skill_level, std::move(movements));
    }

    void MoveMobResponseHandler::handle(InPacket& recv) const
    {
        const int32_t oid = recv.read_int();
        const int16_t moveid = recv.read_short();
        const bool aggro = recv.read_bool();
        recv.read_short(); // authoritative MP; the client only consumes suggestions
        const uint8_t skill_id = recv.read_byte();
        const uint8_t skill_level = recv.read_byte();
        Stage::get().get_mobs().suggest_skill(
            oid, moveid, aggro, skill_id, skill_level);
    }

    void ApplyMobStatusHandler::handle(InPacket& recv) const
    {
        const int32_t oid = recv.read_int();
        recv.skip(8);
        const uint32_t first_mask = static_cast<uint32_t>(recv.read_int());
        const uint32_t second_mask = static_cast<uint32_t>(recv.read_int());
        const size_t status_count = static_cast<size_t>(
            __builtin_popcount(first_mask) + __builtin_popcount(second_mask));
        if (recv.length() < status_count * 8 + 5)
        {
            throw PacketError("Truncated APPLY_MONSTER_STATUS payload");
        }

        std::vector<int16_t> values;
        values.reserve(status_count);
        for (size_t index = 0; index < status_count; ++index)
        {
            values.push_back(recv.read_short());
            const uint16_t source_low = static_cast<uint16_t>(recv.read_short());
            const uint16_t source_high = static_cast<uint16_t>(recv.read_short());
            recv.read_short();
            if (source_low >= 100 && source_low <= 200 && source_high > 0 && source_high < 100)
            {
                Stage::get().get_mobs().show_status_skill(
                    oid, source_low, source_high);
            }
        }
        Stage::get().get_mobs().apply_status(oid, first_mask, second_mask, values);
    }

    void CancelMobStatusHandler::handle(InPacket& recv) const
    {
        const int32_t oid = recv.read_int();
        recv.skip(8);
        const uint32_t first_mask = static_cast<uint32_t>(recv.read_int());
        const uint32_t second_mask = static_cast<uint32_t>(recv.read_int());
        Stage::get().get_mobs().cancel_status(oid, first_mask, second_mask);
    }

    void DamageMonsterHandler::handle(InPacket& recv) const
    {
        if (recv.length() < 17)
        {
            throw PacketError("Truncated DAMAGE_MONSTER payload");
        }

        const int32_t oid = recv.read_int();
        recv.read_byte(); // type: 0 = normal, 1 = friendly mob
        const int32_t damage = recv.read_int();
        const int32_t curhp = recv.read_int();
        const int32_t maxhp = recv.read_int();

        if (damage > 0)
        {
            Stage::get().get_combat().show_damage(oid, damage);
        }

        if (maxhp > 0 && curhp >= 0)
        {
            const int64_t raw = 100LL * curhp / maxhp;
            const int8_t percent = static_cast<int8_t>(
                std::clamp(raw, int64_t{0}, int64_t{100}));
            const uint16_t playerlevel = Stage::get().get_player()
                .get_stats().get_stat(Maplestat::LEVEL);
            Stage::get().get_mobs().send_mobhp(oid, percent, playerlevel);
        }
    }

    void DamagePlayerHandler::handle(InPacket& recv) const
    {
        if (recv.length() < 9)
        {
            throw PacketError("Truncated DAMAGE_PLAYER payload");
        }

        const int32_t cid = recv.read_int();
        const int8_t skill = recv.read_byte();
        if (skill == -3)
        {
            // Fall damage prefixes the damage with a 4-byte zero pad;
            // without skipping it the damage would read as 0 and be
            // dropped by the guard below.
            if (recv.length() < 8)
            {
                throw PacketError("Truncated DAMAGE_PLAYER fall payload");
            }
            recv.skip(4);
        }
        const int32_t damage = recv.read_int();

        if (damage <= 0)
            return;

        if (cid == Stage::get().get_player().get_oid())
        {
            // Contact damage is already shown by the local prediction, so
            // only damage over time (skill -4) is displayed here. The DoT
            // variant omits the trailing monster block; nothing of it is
            // read, so no extra skip is needed.
            if (skill == -4)
                Stage::get().get_player().show_damage(damage);
        }
        else if (Optional<OtherChar> chr = Stage::get().get_chars().get_char(cid))
        {
            chr->show_damage(damage);
        }
    }

    void SpawnMistHandler::handle(InPacket& recv) const
    {
        if (recv.length() < 39)
        {
            throw PacketError("Truncated SPAWN_MIST payload");
        }
        const int32_t oid = recv.read_int();
        const int32_t type = recv.read_int();
        const int32_t owner = recv.read_int();
        const int32_t skill = recv.read_int();
        const uint8_t level = static_cast<uint8_t>(recv.read_byte());
        recv.read_short();
        const int32_t left = recv.read_int();
        const int32_t top = recv.read_int();
        const int32_t right = recv.read_int();
        const int32_t bottom = recv.read_int();
        recv.read_int();
        Stage::get().get_mists().spawn(
            oid, type, owner, skill, level, left, top, right, bottom);
    }

    void RemoveMistHandler::handle(InPacket& recv) const
    {
        Stage::get().get_mists().remove(recv.read_int());
    }


    void ShowMobHpHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        int8_t hppercent = recv.read_byte();
        uint16_t playerlevel = Stage::get().get_player().get_stats().get_stat(Maplestat::LEVEL);

        Stage::get().get_mobs().send_mobhp(oid, hppercent, playerlevel);
    }


    void SpawnNpcHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        int32_t id = recv.read_int();
        Point<int16_t> position = recv.read_point();
        bool flip = recv.read_bool();
        uint16_t fh = recv.read_short();

        recv.read_short(); // 'rx'
        recv.read_short(); // 'ry'

        Stage::get().get_npcs().spawn({
            oid, id, position, flip, fh
        });
    }


    void SpawnNpcControllerHandler::handle(InPacket& recv) const
    {
        int8_t mode = recv.read_byte();
        int32_t oid = recv.read_int();

        if (mode == 0)
        {
            Stage::get().get_npcs().remove(oid);
        }
        else
        {
            int32_t id = recv.read_int();
            Point<int16_t> position = recv.read_point();
            bool flip = recv.read_bool();
            uint16_t fh = recv.read_short();

            recv.read_short(); // 'rx'
            recv.read_short(); // 'ry'
            recv.read_bool(); // 'minimap'

            Stage::get().get_npcs().spawn({
                oid, id, position, flip, fh
            });
        }
    }


    void DropLootHandler::handle(InPacket& recv) const
    {
        int8_t mode = recv.read_byte();
        int32_t oid = recv.read_int();
        bool meso = recv.read_bool();
        int32_t itemid = recv.read_int();
        int32_t owner = recv.read_int();
        int8_t pickuptype = recv.read_byte();
        Point<int16_t> dropto = recv.read_point();

        recv.skip(4);

        Point<int16_t> dropfrom;
        if (mode != 2)
        {
            dropfrom = recv.read_point();

            recv.skip(2);
        }
        else
        {
            dropfrom = dropto;
        }

        if (!meso)
        {
            recv.skip(8);
        }
        bool playerdrop = !recv.read_bool();

        Stage::get().get_drops().spawn({
            oid, itemid, meso, owner, dropfrom, dropto, pickuptype, mode, playerdrop
        });
    }


    void RemoveLootHandler::handle(InPacket& recv) const
    {
        const drop_removal::Payload payload = drop_removal::parse_payload(recv);
        Stage& stage = Stage::get();

        Optional<PhysicsObject> looter;
        std::optional<Point<int16_t>> pickup_target;
        if (payload.effect == drop_removal::Effect::PLAYER_PICKUP)
        {
            if (Optional<Char> character = stage.get_character(payload.character_id))
            {
                looter = character->get_phobj();
            }
        }
        else if (payload.effect == drop_removal::Effect::PET_PICKUP)
        {
            if (Optional<Char> character = stage.get_character(payload.character_id))
            {
                const auto pet_index = static_cast<uint8_t>(payload.pet_index);
                if (const PetLook* pet = character->get_pet(pet_index))
                {
                    pickup_target = pet->get_position();
                }
            }
        }

        if (payload.effect == drop_removal::Effect::PLAYER_PICKUP ||
            payload.effect == drop_removal::Effect::PET_PICKUP)
        {
            Sound(Sound::PICKUP).play();
        }

        if (!payload.known_mode)
        {
            Console::get().print(
                "remove_drop: unknown mode " + std::to_string(payload.mode) +
                " for oid " + std::to_string(payload.oid) +
                "; erasing authoritative drop"
            );
        }

        const bool local_pet_pickup =
            payload.effect == drop_removal::Effect::PET_PICKUP &&
            stage.is_player(payload.character_id);
        stage.get_drops().remove(
            payload.oid,
            payload.effect,
            looter.get(),
            pickup_target,
            local_pet_pickup
        );
    }


    void SpawnReactorHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        int32_t rid = recv.read_int();
        int8_t state = recv.read_byte();
        Point<int16_t> point = recv.read_point();

        // Unused in Solaxia
        // uint16_t fhid = recv.read_short();
        // recv.read_byte()

        Stage::get().get_reactors().spawn({
            oid, rid, state, point
        });
    }


    void RemoveReactorHandler::handle(InPacket& recv) const
    {
        int32_t oid = recv.read_int();
        int8_t state = recv.read_byte();
        Point<int16_t> point = recv.read_point();

        Stage::get().get_reactors()
            .remove(oid, state, point);
    }
}
