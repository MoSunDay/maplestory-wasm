// MovementParser byte-table alignment verification, mirroring the linked
// v83 server MovementPacketDecoder:
// - one packet carrying every movement command 0..22 must be consumed to
//   the last byte (a mis-sized field would under- or overflow the cursor);
// - server-dropped fragments (10 equip change, 14 jump down w/o fh, 21 Aran
//   arcing) must not enter the replay list;
// - command 15 reads fh then the origin foothold, so the replayed foothold
//   is the destination one and the origin bytes are skipped;
// - teleport-shaped commands (3/4/7/8/9/11) carry no duration on the wire
//   and replay with duration 0 as instant waypoints.
// The parser is a pure function over InPacket, so the whole table is
// checked in one pass, plus per-family field round trips.
#include "Net/InPacket.h"
#include "Net/Handlers/Helpers/MovementParser.h"

#include "Gameplay/Movement.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
    using namespace jrc;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << message << '\n';
            std::exit(1);
        }
    }

    using Bytes = std::vector<int8_t>;

    void put_byte(Bytes& stream, uint8_t value)
    {
        stream.push_back(static_cast<int8_t>(value));
    }

    // The wire format is little-endian, like InPacket::read<T>().
    void put_short(Bytes& stream, int16_t value)
    {
        const uint16_t raw = static_cast<uint16_t>(value);
        put_byte(stream, static_cast<uint8_t>(raw & 0xFF));
        put_byte(stream, static_cast<uint8_t>((raw >> 8) & 0xFF));
    }

    // Server byte table: absolute (0/5/17), relative (1/2/6/12/13/16/18/19/
    // 20/22), teleport-shaped (3/4/7/8/9/11), jump down 15, dropped
    // 10/14/21. Field values are distinct per command so a shifted read
    // can never alias another fragment's field.
    void append_absolute(Bytes& stream, uint8_t command)
    {
        put_byte(stream, command);
        put_short(stream, 100 + command);
        put_short(stream, 200 + command);
        put_short(stream, 300 + command);
        put_short(stream, 400 + command);
        put_short(stream, 5 + command);
        put_byte(stream, 1 + command);
        put_short(stream, 80 + command);
    }

    void append_relative(Bytes& stream, uint8_t command)
    {
        put_byte(stream, command);
        put_short(stream, 110 + command);
        put_short(stream, 210 + command);
        put_byte(stream, 2 + command);
        put_short(stream, 90 + command);
    }

    void append_teleport(Bytes& stream, uint8_t command)
    {
        put_byte(stream, command);
        put_short(stream, 120 + command);
        put_short(stream, 220 + command);
        put_short(stream, 320 + command);
        put_short(stream, 420 + command);
        put_byte(stream, 3 + command);
    }

    void append_jump_down(Bytes& stream)
    {
        put_byte(stream, 15);
        put_short(stream, 115);
        put_short(stream, 215);
        put_short(stream, 315);
        put_short(stream, 415);
        put_short(stream, 7);
        put_short(stream, 9);
        put_byte(stream, 16);
        put_short(stream, 95);
    }

    void append_dropped(Bytes& stream, uint8_t command)
    {
        put_byte(stream, command);
        if (command == 14)
        {
            // x y lastx lasty state: read then dropped by the server.
            put_short(stream, 114);
            put_short(stream, 214);
            put_short(stream, 314);
            put_short(stream, 414);
            put_byte(stream, 15);
        }
        else if (command == 21)
        {
            // x state: read then dropped by the server.
            put_short(stream, 121);
            put_byte(stream, 22);
        }
        else
        {
            // 10: one stat byte, looks ride a separate packet.
            put_byte(stream, 23);
        }
    }

    const Movement* find_fragment(const std::vector<Movement>& movements,
        uint8_t command)
    {
        for (const Movement& fragment : movements)
        {
            if (fragment.command == command)
                return &fragment;
        }
        return nullptr;
    }
}

int main()
{
    using namespace jrc;

    // Every command 0..22 in one batch, in wire order.
    Bytes stream;
    put_byte(stream, 23);
    for (uint8_t command = 0; command <= 22; ++command)
    {
        switch (command)
        {
        case 0:
        case 5:
        case 17:
            append_absolute(stream, command);
            break;
        case 1:
        case 2:
        case 6:
        case 12:
        case 13:
        case 16:
        case 18:
        case 19:
        case 20:
        case 22:
            append_relative(stream, command);
            break;
        case 3:
        case 4:
        case 7:
        case 8:
        case 9:
        case 11:
            append_teleport(stream, command);
            break;
        case 15:
            append_jump_down(stream);
            break;
        default:
            append_dropped(stream, command);
            break;
        }
    }

    InPacket packet(stream.data(), stream.size());
    const std::vector<Movement> movements = MovementParser::parse_movements(packet);

    // The cursor must land exactly on the end of the batch: any field-size
    // drift either throws PacketError or leaves bytes behind.
    require(packet.position() == stream.size() && !packet.available(),
        "byte table consumes the whole batch");
    require(movements.size() == 20,
        "only the 20 server-kept commands are enqueued");
    require(find_fragment(movements, 10) == nullptr
        && find_fragment(movements, 14) == nullptr
        && find_fragment(movements, 21) == nullptr,
        "dropped commands 10/14/21 never enter the replay");

    // Absolute family: full field round trip.
    const Movement* absolute = find_fragment(movements, 0);
    require(absolute && absolute->type == Movement::_ABSOLUTE,
        "command 0 parses as absolute");
    require(absolute->xpos == 100 && absolute->ypos == 200
        && absolute->lastx == 300 && absolute->lasty == 400
        && absolute->fh == 5 && absolute->newstate == 1
        && absolute->duration == 80,
        "command 0 fields round trip");
    require(find_fragment(movements, Movement::CMD_SWIM)->type
        == Movement::_ABSOLUTE,
        "swimming command 5 decodes like an absolute move");
    require(find_fragment(movements, 17)->type == Movement::_ABSOLUTE,
        "command 17 decodes like an absolute move");

    // Relative family: x y state duration round trip.
    const Movement* relative = find_fragment(movements, 1);
    require(relative && relative->type == Movement::_RELATIVE
        && relative->xpos == 111 && relative->ypos == 211
        && relative->newstate == 3 && relative->duration == 91,
        "command 1 parses as relative with round-tripped fields");
    for (uint8_t command : { 2, 6, 12, 13, 16, 18, 19, 20, 22 })
    {
        const Movement* fragment = find_fragment(movements, command);
        require(fragment && fragment->type == Movement::_RELATIVE
            && fragment->xpos == 110 + command
            && fragment->duration == 90 + command,
            "new relative commands 18/19/20/22 share the family layout");
    }

    // Teleport family: no duration on the wire, replayed as instants.
    for (uint8_t command : { 3, 4, 7, 8, 9, 11 })
    {
        const Movement* fragment = find_fragment(movements, command);
        require(fragment && fragment->type == Movement::TELEPORT
            && fragment->duration == 0
            && fragment->xpos == 120 + command
            && fragment->newstate == 3 + command,
            "teleport-shaped commands replay with duration 0");
    }

    // Jump down: fh comes from the destination slot, the origin foothold
    // bytes are skipped between fh and state.
    const Movement* jump_down = find_fragment(movements, 15);
    require(jump_down && jump_down->type == Movement::JUMPDOWN,
        "command 15 parses as jump down");
    require(jump_down->fh == 7 && jump_down->fh != 9,
        "command 15 keeps the destination fh, not the origin fh");
    require(jump_down->newstate == 16 && jump_down->duration == 95,
        "command 15 reads state and duration after the origin fh");

    // Unknown command: the server drops the whole movement list when it
    // meets one, so the client must truncate the batch instead of parsing
    // the remaining fragments against a misaligned cursor. The junk bytes
    // behind the unknown command may stay unconsumed; only the fragment
    // count is asserted. Reaching the require at all also proves the
    // parser neither throws nor crashes on the unknown byte.
    Bytes unknown_stream;
    put_byte(unknown_stream, 3);
    append_absolute(unknown_stream, 0);
    put_byte(unknown_stream, 23);
    put_byte(unknown_stream, 0xAA);
    put_byte(unknown_stream, 0xAA);
    put_byte(unknown_stream, 0xAA);
    put_byte(unknown_stream, 0xAA);
    append_absolute(unknown_stream, 5);
    InPacket unknown_packet(unknown_stream.data(), unknown_stream.size());
    const std::vector<Movement> truncated =
        MovementParser::parse_movements(unknown_packet);
    require(truncated.empty(),
        "unknown command 23 truncates the whole fragment batch");

    std::cout << "movement parser alignment verification passed\n";
}
