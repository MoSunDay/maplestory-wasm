#include "MapChars.h"

namespace jrc
{
    void MapChars::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        chars.draw(layer, viewx, viewy, alpha);
    }

    void MapChars::update(const Physics& physics)
    {
        instantiate_spawns();
        chars.update(physics);
    }

    void MapChars::instantiate_spawns()
    {
        for (; !spawns.empty(); spawns.pop())
        {
            const CharSpawn& spawn = spawns.front();

            int32_t cid = spawn.get_cid();
            Optional<OtherChar> ochar = get_char(cid);
            if (ochar)
            {
                // The server re-sends SPAWN_PLAYER when the character
                // changes map or needs correcting: refresh the known
                // character in place instead of dropping it.
                ochar->respawn(spawn.get_position(), spawn.get_stance());
                apply_pets(*ochar, spawn.get_pets(), spawn.get_pet_count());
                replay_pending_pets(cid, *ochar);
            }
            else
            {
                chars.add(
                    spawn.instantiate()
                );
                if (Optional<OtherChar> added = get_char(cid))
                {
                    apply_pets(*added, spawn.get_pets(), spawn.get_pet_count());
                    replay_pending_pets(cid, *added);
                }
            }
        }

    }

    void MapChars::spawn(CharSpawn&& spawn)
    {
        spawns.emplace(
            std::move(spawn)
        );
    }

    void MapChars::remove(int32_t cid)
    {
        // A REMOVE_CHAR can arrive in the same network batch as the queued
        // SPAWN_PLAYER; drop pending spawns for this cid so a character
        // that left the map cannot be brought back by a stale entry.
        std::queue<CharSpawn> retained;
        for ( ; !spawns.empty(); spawns.pop())
        {
            if (spawns.front().get_cid() != cid)
                retained.emplace(std::move(spawns.front()));
        }
        spawns = std::move(retained);

        chars.remove(cid);

        pending_pets.erase(cid);
    }

    void MapChars::clear()
    {
        chars.clear();

        // Cross-map packets can queue spawns from the previous map; a fresh
        // map must not instantiate stale spawn entries.
        spawns = std::queue<CharSpawn>{};

        pending_pets.clear();
    }

    bool MapChars::has_pending_spawn(int32_t cid) const
    {
        // The queue has no iteration API; copies stay tiny (map enter batches).
        std::queue<CharSpawn> scan = spawns;
        for (; !scan.empty(); scan.pop())
        {
            if (scan.front().get_cid() == cid)
                return true;
        }
        return false;
    }

    void MapChars::stash_pet(int32_t cid, const PetUpdate& update)
    {
        // SPAWN_PET can share a network batch with the owner's SPAWN_PLAYER;
        // keep the update for the instantiation pass instead of dropping it.
        if (has_pending_spawn(cid))
            pending_pets[cid].push_back(update);
    }

    void MapChars::apply_pets(Char& character, const PetSpawnEntry* pets, uint8_t count)
    {
        // SPAWN_PLAYER carries the owner's full summon state: sync all slots.
        for (uint8_t i = 0; i < 3; ++i)
            character.remove_pet(i, false);
        for (uint8_t i = 0; i < count && i < 3; ++i)
        {
            const PetSpawnEntry& pet = pets[i];
            character.add_pet(i, pet.itemid, pet.name, pet.uniqueid,
                pet.position, pet.stance, pet.foothold);
        }
    }

    void MapChars::replay_pending_pets(int32_t cid, Char& character)
    {
        auto iter = pending_pets.find(cid);
        if (iter == pending_pets.end())
            return;
        for (const PetUpdate& update : iter->second)
        {
            if (update.despawn)
                character.remove_pet(update.index, update.hunger);
            else
                character.add_pet(update.index, update.pet.itemid,
                    update.pet.name, update.pet.uniqueid, update.pet.position,
                    update.pet.stance, update.pet.foothold);
        }
        pending_pets.erase(iter);
    }

    void MapChars::send_movement(int32_t cid, const std::vector<Movement>& movements)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->send_movement(movements);
        }
    }

    void MapChars::update_look(int32_t cid, const LookEntry& look)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->update_look(look);
        }
    }

    void MapChars::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->set_party_hp(hp, max_hp);
        }
    }

    void MapChars::clear_party_member_hp()
    {
        for (auto& iter : chars)
        {
            Optional<OtherChar> otherchar = iter.second.get();
            if (otherchar)
            {
                otherchar->clear_party_hp();
            }
        }
    }

    std::optional<PlayerTarget> MapChars::find_target(
        Point<int16_t> cursor,
        Point<int16_t> view_position) const
    {
        return find_target_from(cursor, view_position, nullptr);
    }

    std::optional<PlayerTarget> MapChars::find_target_with(
        Point<int16_t> cursor,
        Point<int16_t> view_position,
        const PlayerTarget& additional_target) const
    {
        return find_target_from(cursor, view_position, &additional_target);
    }

    std::optional<PlayerTarget> MapChars::find_target_from(
        Point<int16_t> cursor,
        Point<int16_t> view_position,
        const PlayerTarget* additional_target) const
    {
        std::vector<PlayerTarget> candidates;
        candidates.reserve(chars.size() + (additional_target ? 1 : 0));

        if (additional_target)
            candidates.push_back(*additional_target);

        for (const auto& iter : chars)
        {
            const OtherChar* character = static_cast<const OtherChar*>(iter.second.get());
            if (character && character->is_active())
            {
                candidates.push_back({
                    character->get_oid(),
                    character->get_name(),
                    character->get_level(),
                    character->get_position()
                });
            }
        }

        return pick_player_target(candidates, cursor, view_position);
    }

    void MapChars::set_visual_buff(int32_t cid, Buffstat::Id stat, int16_t value)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->set_visual_buff(stat, value);
    }

    void MapChars::set_visual_buff_source(int32_t cid, Buffstat::Id stat, int32_t source)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->set_visual_buff_source(stat, source);
    }

    void MapChars::cancel_visual_buff(int32_t cid, Buffstat::Id stat)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->cancel_visual_buff(stat);
    }

    Optional<OtherChar> MapChars::get_char(int32_t cid)
    {
        return chars.get(cid);
    }
}
