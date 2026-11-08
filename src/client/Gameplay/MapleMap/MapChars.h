#pragma once
#include "MapObjects.h"

#include "../Movement.h"
#include "../Spawn.h"
#include "../Targeting/PlayerTargeting.h"

#include "../../Character/Look/PetSpawnEntry.h"
#include "../../Character/OtherChar.h"

#include <map>
#include <queue>
#include <optional>
#include <vector>

namespace jrc
{
    // A collection of remote controlled characters on a map.
    class MapChars
    {
    public:
        // Draw all characters on a layer.
        void draw(Layer::Id layer, double viewx, double viewy, float alpha) const;
        // Update all characters.
        void update(const Physics& physics);
        void instantiate_spawns();

        // Spawn a new character, if it has not been spawned yet.
        void spawn(CharSpawn&& spawn);
        // Remove a character.
        void remove(int32_t cid);
        // Remove all characters.
        void clear();

        // A pet update parsed from SPAWN_PET, replayed once the owner exists.
        struct PetUpdate
        {
            uint8_t index;
            bool despawn;
            bool hunger;
            PetSpawnEntry pet;
        };

        // Stash a pet update for a character that is still queued for
        // instantiation; dropped when the cid is not pending after all.
        void stash_pet(int32_t cid, const PetUpdate& update);

        // Update a characters movement.
        void send_movement(int32_t cid, const std::vector<Movement>& movements);
        // Update a characters look.
        void update_look(int32_t cid, const LookEntry& look);
        // Update a party member hp bar for an on-map character.
        void update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp);
        // Hide all on-map party member hp bars.
        void clear_party_member_hp();
        // Return the visible remote player under a screen-space cursor.
        std::optional<PlayerTarget> find_target(
            Point<int16_t> cursor,
            Point<int16_t> view_position
        ) const;
        std::optional<PlayerTarget> find_target_with(
            Point<int16_t> cursor,
            Point<int16_t> view_position,
            const PlayerTarget& additional_target
        ) const;
        void set_visual_buff(int32_t cid, Buffstat::Id stat, int16_t value);
        void set_visual_buff_source(int32_t cid, Buffstat::Id stat, int32_t source);
        void cancel_visual_buff(int32_t cid, Buffstat::Id stat);

        Optional<OtherChar> get_char(int32_t cid);

        MapObjects* get_chars()
        {
            return &chars;
        }

        const MapObjects* get_chars() const
        {
            return &chars;
        }

    private:
        std::optional<PlayerTarget> find_target_from(
            Point<int16_t> cursor,
            Point<int16_t> view_position,
            const PlayerTarget* additional_target
        ) const;
        bool has_pending_spawn(int32_t cid) const;
        void apply_pets(Char& character, const PetSpawnEntry* pets, uint8_t count);
        void replay_pending_pets(int32_t cid, Char& character);
        MapObjects chars;

        std::queue<CharSpawn> spawns;
        std::map<int32_t, std::vector<PetUpdate>> pending_pets;
    };
}
