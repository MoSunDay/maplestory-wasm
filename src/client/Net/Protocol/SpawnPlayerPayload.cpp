#include "SpawnPlayerPayload.h"

#include "../Handlers/Helpers/LoginParser.h"

#include <cstddef>

namespace jrc::spawn_player
{
    namespace
    {
        constexpr int32_t MORPHED_MARKER = 2;
        constexpr int32_t COMBO_HIGH_MASK = 0x00200000;
        constexpr int32_t ENERGY_CHARGE_HIGH_MASK = 0x00040000;

        void skip_ring(InPacket& recv)
        {
            if (recv.read_byte() == 1)
            {
                // The server writes one equipped ring as five ints.
                recv.skip(20);
            }
        }

        void skip_announce_box(InPacket& recv)
        {
            const int8_t type = recv.read_byte();
            if (type == 0)
            {
                return;
            }

            recv.read_int();    // shop / minigame object id
            recv.read_string(); // description

            if (type == 4)
            {
                recv.skip(5); // piece, amount, capacity, joinable, trailing
            }
            else
            {
                recv.read_bool(); // password protected
                recv.skip(4);     // piece, amount, capacity, joinable
            }
        }

        void skip_new_year_cards(InPacket& recv)
        {
            if (recv.read_byte() != 1)
            {
                return;
            }

            const int32_t count = recv.read_int();
            if (count < 0 || static_cast<size_t>(count) > recv.length() / sizeof(int32_t))
            {
                throw PacketError("Invalid new-year card count in SPAWN_PLAYER");
            }
            recv.skip(static_cast<size_t>(count) * sizeof(int32_t));
        }
    }

    Payload parse_payload(InPacket& recv)
    {
        Payload payload;
        payload.header = parse_header(recv);

        recv.read_string(); // guild name
        recv.read_short();  // guild logo background
        recv.read_byte();   // guild logo background color
        recv.read_short();  // guild logo
        recv.read_byte();   // guild logo color
        recv.skip(8);       // zeroes + v83 spawn marker

        const bool morphed = recv.read_int() == MORPHED_MARKER;
        const int32_t buffmask1 = recv.read_int();

        // The linked server writes at most one foreign-buff value. Morph
        // values are shorts; combo and energy charge values are bytes.
        // Darksight, soul arrow and shadow partner only set mask bits.
        if (morphed)
        {
            recv.read_short();
        }
        else if (buffmask1 & (COMBO_HIGH_MASK | ENERGY_CHARGE_HIGH_MASK))
        {
            const int8_t value = recv.read_byte();
            if (buffmask1 & COMBO_HIGH_MASK)
            {
                payload.combo_value = value;
            }
        }

        recv.read_int(); // buffmask2 (debuffs are sent separately)
        recv.skip(43);

        recv.read_int(); // mount item id
        recv.skip(61);

        payload.job = recv.read_short();
        payload.look = LoginParser::parse_look(recv);

        recv.read_int(); // cash item count (5110000)
        recv.read_int(); // item effect
        recv.read_int(); // chair id

        payload.position = recv.read_point();
        payload.stance = recv.read_byte();
        recv.skip(3); // zero foothold + unk

        // Pets are written as: 1, itemid, name, 8-byte uid, pos,
        // stance, fh for each active pet, followed by one 0 terminator.
        for (uint8_t i = 0; i < 4; ++i)
        {
            if (recv.read_byte() != 1)
            {
                break;
            }
            PetSpawnEntry& pet =
                payload.pets[payload.pet_count < 3 ? payload.pet_count : 2];
            pet.itemid = recv.read_int();
            pet.name = recv.read_string();
            // The wire unique id is 8 bytes; the low half identifies the pet.
            pet.uniqueid = static_cast<int32_t>(recv.read_long());
            pet.position = recv.read_point();
            pet.stance = recv.read_byte();
            pet.foothold = recv.read_int();
            if (payload.pet_count < 3)
                ++payload.pet_count;
        }

        recv.read_int(); // mount level
        recv.read_int(); // mount exp
        recv.read_int(); // mount tiredness

        skip_announce_box(recv);

        if (recv.read_bool())
        {
            recv.read_string(); // chalkboard text
        }

        skip_ring(recv); // crush ring
        skip_ring(recv); // friendship ring

        if (recv.read_byte() == 1)
        {
            recv.skip(12); // marriage ring: two character ids + item id
        }

        skip_new_year_cards(recv);

        recv.skip(2);
        recv.read_byte(); // team

        return payload;
    }
}
