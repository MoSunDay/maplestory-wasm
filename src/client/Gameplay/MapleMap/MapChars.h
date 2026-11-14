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
        void instantiate_spawns(const Physics& physics);

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

        // A remote character event that arrived before its owner left the
        // spawn queue; replayed in arrival order once the owner exists.
        struct CharEvent
        {
            enum Kind
            {
                MOVE,       // MOVE_PLAYER fragments
                PET_MOVE,   // MOVE_PET fragments for one pet slot
                LOOK,       // UPDATE_CHARLOOK payload
                EXPRESSION, // FACIAL_EXPRESSION emote id
                CHAIR,      // SHOW_CHAIR item id
                CHALKBOARD  // CHALKBOARD board text
            };

            Kind kind = MOVE;
            uint8_t pet_index = 0;           // PET_MOVE
            int32_t value = 0;               // EXPRESSION emote / CHAIR item id
            std::string text;                // CHALKBOARD
            std::vector<Movement> movements; // MOVE / PET_MOVE
            LookEntry look;                  // LOOK
        };

        // Stash an event for a character that is still queued for
        // instantiation; dropped when the cid is not pending after all.
        // A newer event of the same kind replaces the stashed one, as all
        // five kinds carry idempotent character state.
        void stash_event(int32_t cid, const CharEvent& event);

        // Update a characters movement; stashed while the cid is queued.
        void send_movement(int32_t cid, const std::vector<Movement>& movements);
        // Update a pets movement; stashed while the owner cid is queued.
        void move_pet(
            int32_t cid,
            uint8_t index,
            const std::vector<Movement>& movements
        );
        // Update a characters look; stashed while the cid is queued.
        void update_look(int32_t cid, const LookEntry& look);
        // Play an emote on a character; stashed while the cid is queued.
        void set_expression(int32_t cid, int32_t emote);
        // Sit a character down in a chair; stashed while the cid is queued.
        void set_chair(int32_t cid, int32_t item_id);
        // Set a character's chalkboard text; an empty string closes it.
        // Stashed while the cid is queued.
        void set_chalkboard(int32_t cid, const std::string& text);
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

        // True while a SPAWN_PLAYER for this cid is still queued, i.e. the
        // character has not been instantiated on the map yet.
        bool has_pending_spawn(int32_t cid) const;

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
        void apply_pets(Char& character, const PetSpawnEntry* pets, uint8_t count);
        void replay_pending_pets(int32_t cid, Char& character);
        void replay_pending_events(int32_t cid, OtherChar& character);
        MapObjects chars;

        std::queue<CharSpawn> spawns;
        std::map<int32_t, std::vector<PetUpdate>> pending_pets;
        std::map<int32_t, std::vector<CharEvent>> pending_events;
    };
}
