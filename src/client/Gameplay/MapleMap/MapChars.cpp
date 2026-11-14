#include "MapChars.h"

#include <algorithm>

namespace jrc
{
    namespace
    {
        // A queued character only ever replays the newest state per kind:
        // movements, emotes, chairs and looks are all idempotent state.
        constexpr size_t MAX_STASHED_EVENTS_PER_CID = 4;

        bool same_event_target(
            const MapChars::CharEvent& lhs,
            const MapChars::CharEvent& rhs
        )
        {
            return lhs.kind == rhs.kind
                && (lhs.kind != MapChars::CharEvent::PET_MOVE
                    || lhs.pet_index == rhs.pet_index);
        }
    }

    void MapChars::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        chars.draw(layer, viewx, viewy, alpha);
    }

    void MapChars::update(const Physics& physics)
    {
        instantiate_spawns(physics);
        chars.update(physics);
    }

    void MapChars::instantiate_spawns(const Physics& physics)
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
                ochar->respawn(
                    spawn.resolve_position(physics), spawn.get_stance());
                ochar->set_chair(spawn.get_chair_id());
                ochar->set_chalkboard(spawn.get_chalkboard());
                apply_pets(*ochar, spawn.get_pets(), spawn.get_pet_count());
                replay_pending_pets(cid, *ochar);
                replay_pending_events(cid, *ochar);
            }
            else
            {
                chars.add(
                    spawn.instantiate(physics)
                );
                if (Optional<OtherChar> added = get_char(cid))
                {
                    added->set_chair(spawn.get_chair_id());
                    added->set_chalkboard(spawn.get_chalkboard());
                    apply_pets(*added, spawn.get_pets(), spawn.get_pet_count());
                    replay_pending_pets(cid, *added);
                    replay_pending_events(cid, *added);
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
        pending_events.erase(cid);
    }

    void MapChars::clear()
    {
        chars.clear();

        // Cross-map packets can queue spawns from the previous map; a fresh
        // map must not instantiate stale spawn entries.
        spawns = std::queue<CharSpawn>{};

        pending_pets.clear();
        pending_events.clear();
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

    void MapChars::stash_event(int32_t cid, const CharEvent& event)
    {
        // Movement, look, emote and chair packets can share a network batch
        // with the owner's SPAWN_PLAYER; keep the newest state for the
        // instantiation pass instead of dropping it. Unknown cids are not
        // stashed, and the per-cid cap bounds a cid that never instantiates.
        if (!has_pending_spawn(cid))
            return;

        std::vector<CharEvent>& events = pending_events[cid];
        auto stashed = std::find_if(
            events.begin(),
            events.end(),
            [&event](const CharEvent& entry)
            {
                return same_event_target(entry, event);
            }
        );
        if (stashed != events.end())
            *stashed = event;
        else
            events.push_back(event);

        if (events.size() > MAX_STASHED_EVENTS_PER_CID)
            events.erase(events.begin());
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

    void MapChars::replay_pending_events(int32_t cid, OtherChar& character)
    {
        auto iter = pending_events.find(cid);
        if (iter == pending_events.end())
            return;
        for (const CharEvent& event : iter->second)
        {
            switch (event.kind)
            {
                case CharEvent::MOVE:
                    character.send_movement(event.movements);
                    break;
                case CharEvent::PET_MOVE:
                    character.move_pet(event.pet_index, event.movements);
                    break;
                case CharEvent::LOOK:
                    character.update_look(event.look);
                    break;
                case CharEvent::EXPRESSION:
                    character.set_expression(event.value);
                    break;
                case CharEvent::CHAIR:
                    character.set_chair(event.value);
                    character.set_state(
                        event.value > 0 ? Char::SIT : Char::STAND
                    );
                    break;
                case CharEvent::CHALKBOARD:
                    character.set_chalkboard(event.text);
                    break;
            }
        }
        pending_events.erase(iter);
    }

    void MapChars::send_movement(int32_t cid, const std::vector<Movement>& movements)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->send_movement(movements);
        }
        else
        {
            // The character is still queued: stash the fragments so its
            // first frame after instantiation matches the broadcast state.
            CharEvent event{};
            event.kind = CharEvent::MOVE;
            event.movements = movements;
            stash_event(cid, event);
        }
    }

    void MapChars::move_pet(
        int32_t cid,
        uint8_t index,
        const std::vector<Movement>& movements
    )
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->move_pet(index, movements);
        }
        else
        {
            CharEvent event{};
            event.kind = CharEvent::PET_MOVE;
            event.pet_index = index;
            event.movements = movements;
            stash_event(cid, event);
        }
    }

    void MapChars::update_look(int32_t cid, const LookEntry& look)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->update_look(look);
        }
        else
        {
            CharEvent event{};
            event.kind = CharEvent::LOOK;
            event.look = look;
            stash_event(cid, event);
        }
    }

    void MapChars::set_expression(int32_t cid, int32_t emote)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->set_expression(emote);
        }
        else
        {
            CharEvent event{};
            event.kind = CharEvent::EXPRESSION;
            event.value = emote;
            stash_event(cid, event);
        }
    }

    void MapChars::set_chair(int32_t cid, int32_t item_id)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->set_chair(item_id);
            otherchar->set_state(item_id > 0 ? Char::SIT : Char::STAND);
        }
        else
        {
            CharEvent event{};
            event.kind = CharEvent::CHAIR;
            event.value = item_id;
            stash_event(cid, event);
        }
    }

    void MapChars::set_chalkboard(int32_t cid, const std::string& text)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->set_chalkboard(text);
        }
        else
        {
            CharEvent event{};
            event.kind = CharEvent::CHALKBOARD;
            event.text = text;
            stash_event(cid, event);
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
