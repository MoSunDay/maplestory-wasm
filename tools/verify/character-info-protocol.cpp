#include "../../src/client/Net/CharacterInfo/CharacterInfoProtocol.h"
#include "../../src/client/Net/PacketError.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void append_byte(std::vector<int8_t>& bytes, int32_t value)
    {
        bytes.push_back(static_cast<int8_t>(value));
    }

    void append_short(std::vector<int8_t>& bytes, int32_t value)
    {
        append_byte(bytes, value);
        append_byte(bytes, value >> 8);
    }

    void append_int(std::vector<int8_t>& bytes, int32_t value)
    {
        for (int index = 0; index < 4; ++index)
        {
            append_byte(bytes, value);
            value >>= 8;
        }
    }

    void append_string(std::vector<int8_t>& bytes, const std::string& value)
    {
        append_short(bytes, static_cast<int32_t>(value.size()));
        for (char byte : value)
            append_byte(bytes, byte);
    }

    void append_pet(std::vector<int8_t>& bytes, int index)
    {
        append_byte(bytes, index + 1);
        append_int(bytes, 5000001 + index);
        append_string(bytes, "pet" + std::to_string(index + 1));
        append_byte(bytes, 10 + index);
        append_short(bytes, 120 + index);
        append_byte(bytes, 80 - index);
        append_short(bytes, 0);
        append_int(bytes, 1802000 + index);
    }

    std::vector<int8_t> character_payload(int pet_count)
    {
        std::vector<int8_t> bytes;
        append_int(bytes, 1234);
        append_short(bytes, 200);
        append_short(bytes, 112);
        append_short(bytes, -7);
        append_byte(bytes, 1);
        append_string(bytes, "Guild");
        append_string(bytes, "Alliance");
        append_byte(bytes, 0);
        for (int index = 0; index < pet_count; ++index)
            append_pet(bytes, index);
        append_byte(bytes, 0);
        append_byte(bytes, 2);
        append_int(bytes, 4);
        append_int(bytes, 500);
        append_int(bytes, 12);
        append_byte(bytes, 2);
        append_int(bytes, 10001);
        append_int(bytes, 10002);
        append_int(bytes, 3);
        append_int(bytes, 4);
        append_int(bytes, 5);
        append_int(bytes, 9);
        append_int(bytes, 9300012);
        append_int(bytes, 1142000);
        append_short(bytes, 2);
        append_short(bytes, 29000);
        append_short(bytes, 29001);
        return bytes;
    }

    bool parse_rejected(std::vector<int8_t> bytes)
    {
        try
        {
            jrc::InPacket packet(bytes.data(), bytes.size());
            (void)jrc::character_info::parse_character_info(packet);
            return false;
        }
        catch (const jrc::PacketError&)
        {
            return true;
        }
    }

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL " << message << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;
    for (int count : { 0, 1, 3 })
    {
        std::vector<int8_t> bytes = character_payload(count);
        jrc::InPacket packet(bytes.data(), bytes.size());
        const auto info = jrc::character_info::parse_character_info(packet);
        ok &= expect(info.character_id == 1234 && info.level == 200 &&
            info.job_id == 112 && info.fame == -7,
            "custom-client short character fields");
        ok &= expect(info.pets.size() == static_cast<size_t>(count),
            "zero, one, or three pets parse");
        ok &= expect(info.mount.marker == 2 && info.mount.experience == 500,
            "mount tail parses");
        ok &= expect(info.wishlist.size() == 2 && info.total_cards == 9 &&
            info.medal_quests.size() == 2,
            "wishlist, monster book, and medal tail parses");
    }

    std::vector<int8_t> truncated = character_payload(1);
    truncated.pop_back();
    ok &= expect(parse_rejected(truncated), "truncated payload rejected");
    std::vector<int8_t> trailing = character_payload(0);
    trailing.push_back(1);
    ok &= expect(parse_rejected(trailing), "trailing payload rejected");

    std::vector<int8_t> fourth;
    append_int(fourth, 1);
    append_short(fourth, 1);
    append_short(fourth, 0);
    append_short(fourth, 0);
    append_byte(fourth, 0);
    append_string(fourth, "");
    append_string(fourth, "");
    append_byte(fourth, 0);
    for (int index = 0; index < 4; ++index)
        append_pet(fourth, index);
    ok &= expect(parse_rejected(fourth), "fourth pet rejected");

    std::vector<int8_t> fame_success;
    append_byte(fame_success, 0);
    append_string(fame_success, "Target");
    append_byte(fame_success, 1);
    append_short(fame_success, 42);
    append_short(fame_success, 0);
    jrc::InPacket fame_packet(fame_success.data(), fame_success.size());
    const auto fame = jrc::character_info::parse_fame_response(fame_packet);
    ok &= expect(fame.status == jrc::character_info::FameStatus::SUCCESS &&
        fame.increase && fame.new_fame == 42, "fame success fields parse");

    for (int status : { 1, 2, 3, 4, 6 })
    {
        const std::vector<int8_t> bytes = { static_cast<int8_t>(status) };
        jrc::InPacket packet(bytes.data(), bytes.size());
        ok &= expect(static_cast<int>(
            jrc::character_info::parse_fame_response(packet).status) == status,
            "fame error status parses");
    }
    std::vector<int8_t> received;
    append_byte(received, 5);
    append_string(received, "Source");
    append_byte(received, 0);
    jrc::InPacket received_packet(received.data(), received.size());
    const auto incoming = jrc::character_info::parse_fame_response(received_packet);
    ok &= expect(incoming.character_name == "Source" && !incoming.increase,
        "received defame parses");

    const std::vector<int8_t> request =
        jrc::character_info::encode_character_info_request(0x12345678);
    ok &= expect(request == std::vector<int8_t>({ 0, 0, 0, 0,
        0x78, 0x56, 0x34, 0x12 }), "character info request wire order");
    const std::vector<int8_t> fame_up =
        jrc::character_info::encode_give_fame(0x12345678, true);
    const std::vector<int8_t> fame_down =
        jrc::character_info::encode_give_fame(0x12345678, false);
    ok &= expect(fame_up == std::vector<int8_t>({
        0x78, 0x56, 0x34, 0x12, 1 }) && fame_down.back() == 0,
        "fame target and increase/decrease mode wire order");

    if (ok)
        std::cout << "PASS character info, pet detail, and fame protocol\n";
    return ok ? 0 : 1;
}
