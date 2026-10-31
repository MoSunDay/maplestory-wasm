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

// Standalone verifier for the cash (NX) equip policy:
//   * items flagged `info/cash` in Character.nx sort into the CASH inventory
//     via InventoryType::by_item_id, while normal equips stay in EQUIP;
//   * a CASH-tab packet entry parses with the equip wire format (ItemParser::
//     parse_item) and lands in the CASH tab marked as a cash item;
//   * wearing it (MOVE_EQUIP to the hidden 100+ slot) moves it to the EQUIP
//     tab instead of EQUIPPED, so recalc_stats never sums its stats;
//   * a normal equip in the same visible slot still contributes stats;
//   * taking it off (MOVE_UNEQUIP) returns it to the CASH tab;
//   * the SET_FIELD cash-equipped segment serializes worn cash equips as
//     negative -100-ish slots which the parser keeps in hidden EQUIP slots;
//   * weapon covers are cosmetic only: a real weapon parsed into the EQUIPPED
//     weapon slot (slot -11) supplies the gameplay weapon type and WATK for
//     stat totals, while a worn cover parses into the hidden EQUIP slot
//     100 + WEAPON (slot -111) and its WATK never reaches the totals;
//   * a cover id (170xxxx) alone maps to Weapon::CASH via Weapon::by_value,
//     which is not a gameplay weapon type, so covers can never pose as the
//     real weapon for damage formulas or attack speed.
//
// Real Character.nx data is used: the first cash cap and the first non-cash
// cap found under character/Cap drive the fake packets, and the first real
// weapon and first cash cover under character/Weapon drive the cover checks.
//
// Usage (from the repository root; clang++ is required because GCC rejects
// Cache<ItemData>'s unordered_map member while ItemData is still incomplete,
// while the WASM build's emscripten/clang frontend defers it):
//   clang++ -std=c++17 -Wno-c++11-narrowing -Isrc/client -Isrc \
//       -Isrc/client/includes tools/verify/cash-equip-policy.cpp \
//       src/client/Character/Inventory/Inventory.cpp \
//       src/client/Character/Inventory/InventoryType.cpp \
//       src/client/Character/Inventory/Weapon.cpp \
//       src/client/Character/Inventory/Equip.cpp \
//       src/client/Character/Inventory/EquipQuality.cpp \
//       src/client/Character/Inventory/Item.cpp \
//       src/client/Character/Inventory/Pet.cpp \
//       src/client/Net/Handlers/Helpers/ItemParser.cpp \
//       src/client/Net/InPacket.cpp \
//       src/client/Data/EquipData.cpp src/client/Data/ItemData.cpp \
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp \
//       src/nlnx/bitmap.cpp src/nlnx/nx.cpp src/client/includes/lz4.c \
//       -o /tmp/cash-equip-policy
//   /tmp/cash-equip-policy assets

#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#include "../../src/client/Character/EquipStat.h"
#include "../../src/client/Character/Inventory/Inventory.h"
#include "../../src/client/Character/Inventory/InventoryType.h"
#include "../../src/client/Character/Inventory/Weapon.h"
#include "../../src/client/Character/Look/EquipSlot.h"
#include "../../src/client/Data/BulletData.h"
#include "../../src/client/Graphics/Animation.h"
#include "../../src/client/Graphics/Texture.h"
#include "../../src/client/Net/Handlers/Helpers/ItemParser.h"
#include "../../src/client/Net/InPacket.h"
#include "../../src/nlnx/nx.hpp"

// Metadata-only stubs, mirroring the npc-embed-path-nx.cpp convention: the
// policy under test never renders, and the real Texture.cpp/BulletData.cpp
// pull in GraphicsGL/FreeType and the bullet Animation, which are unrelated
// to inventory state and unavailable in this bare build environment. Only the
// texture-less data paths of the real translation units are linked above.
namespace jrc
{
    Texture::Texture() {}
    Texture::Texture(nl::node, Texture::LoadMode) {}
    Texture::~Texture() {}
    Animation::Animation() {}
    BulletData::BulletData(int32_t id)
        : itemdata(ItemData::get(id)), watk(0) {}

    int16_t BulletData::get_watk() const
    {
        return watk;
    }
}

namespace
{
    using Bytes = std::vector<int8_t>;

    bool expect(bool condition, const char* what)
    {
        if (!condition)
            std::cerr << "FAIL " << what << '\n';
        return condition;
    }

    template <typename T>
    void append_number(Bytes& bytes, T value)
    {
        using Unsigned = typename std::make_unsigned<T>::type;
        const auto encoded = static_cast<Unsigned>(value);
        for (size_t i = 0; i < sizeof(T); ++i)
            bytes.push_back(static_cast<int8_t>(encoded >> (i * 8)));
    }

    void append_byte(Bytes& bytes, int8_t value)
    {
        append_number(bytes, value);
    }

    void append_short(Bytes& bytes, int16_t value)
    {
        append_number(bytes, value);
    }

    void append_int(Bytes& bytes, int32_t value)
    {
        append_number(bytes, value);
    }

    void append_long(Bytes& bytes, int64_t value)
    {
        append_number(bytes, value);
    }

    void append_string(Bytes& bytes, const std::string& value)
    {
        append_short(bytes, static_cast<int16_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    // Mirrors the wire layout consumed by ItemParser::add_equip.
    void append_equip_entry(Bytes& bytes, int32_t itemid, bool cash,
        int64_t cash_id, uint16_t str_inc, uint16_t watk = 0)
    {
        append_byte(bytes, 1);
        append_int(bytes, itemid);
        append_byte(bytes, cash ? 1 : 0);
        if (cash)
            append_long(bytes, cash_id);
        append_long(bytes, 0);
        append_byte(bytes, 0);
        append_byte(bytes, 0);
        // The equip stats follow the Equipstat order STR, DEX, INT, LUK, HP,
        // MP, WATK, ... on the wire, so WATK rides the short at index
        // Equipstat::WATK (the one after the five stats following STR).
        for (size_t i = 0; i < jrc::Equipstat::LENGTH; ++i)
        {
            const uint16_t value = i == static_cast<size_t>(jrc::Equipstat::WATK)
                ? watk
                : i == 0 ? str_inc : 0;
            append_short(bytes, static_cast<int16_t>(value));
        }
        append_string(bytes, "");
        append_short(bytes, 0);
        if (cash)
        {
            for (int i = 0; i < 10; ++i)
                append_byte(bytes, 0x40);
        }
        else
        {
            append_byte(bytes, 0);
            append_byte(bytes, 0);
            append_short(bytes, 0);
            append_short(bytes, 0);
            append_int(bytes, 0);
            append_long(bytes, 0);
        }
        for (int i = 0; i < 12; ++i)
            append_byte(bytes, 0);
    }

    bool is_cap_image(const std::string& name)
    {
        return name.size() == 12 && name.substr(8) == ".img";
    }

    int32_t cap_id(const std::string& name)
    {
        return std::stoi(name.substr(0, 8));
    }

    // Find the first cash cap and the first normal cap in Character.nx.
    std::pair<int32_t, int32_t> find_sample_caps()
    {
        int32_t cash_hat = 0;
        int32_t normal_hat = 0;
        for (nl::node child : nl::nx::character["Cap"])
        {
            const std::string name = child.name();
            if (!is_cap_image(name))
                continue;
            const bool cash = child["info"]["cash"].get_bool();
            if (cash)
            {
                if (cash_hat == 0)
                    cash_hat = cap_id(name);
            }
            else if (normal_hat == 0)
            {
                normal_hat = cap_id(name);
            }
            if (cash_hat != 0 && normal_hat != 0)
                break;
        }
        return { cash_hat, normal_hat };
    }

    bool is_weapon_image(const std::string& name)
    {
        return name.size() == 12 && name.substr(8) == ".img";
    }

    int32_t weapon_id(const std::string& name)
    {
        return std::stoi(name.substr(0, 8));
    }

    // Find the first real gameplay weapon and the first cash weapon cover
    // (170xxxx) in Character.nx. A cover id maps to Weapon::CASH through
    // Weapon::by_value, which carries no gameplay weapon data. The Weapon
    // directory also holds cash-flagged oddities (prefixes 102/160 and cash
    // event variants of real weapon types), so the cover must map to
    // Weapon::CASH to count.
    std::pair<int32_t, int32_t> find_sample_weapons()
    {
        int32_t real_weapon = 0;
        int32_t cash_cover = 0;
        for (nl::node child : nl::nx::character["Weapon"])
        {
            const std::string name = child.name();
            if (!is_weapon_image(name))
                continue;
            const int32_t id = weapon_id(name);
            const jrc::Weapon::Type type = jrc::Weapon::by_value(id / 10000);
            const bool cash = child["info"]["cash"].get_bool();
            const bool gameplay = type != jrc::Weapon::NONE
                && type != jrc::Weapon::CASH;
            if (cash && type == jrc::Weapon::CASH)
            {
                if (cash_cover == 0)
                    cash_cover = id;
            }
            else if (real_weapon == 0 && !cash && gameplay)
            {
                real_weapon = id;
            }
            if (real_weapon != 0 && cash_cover != 0)
                break;
        }
        return { real_weapon, cash_cover };
    }
}

int main(int argc, char** argv)
{
    using namespace jrc;

    const char* assets = argc > 1 ? argv[1] : "assets";
    if (chdir(assets) != 0)
    {
        std::cerr << "FAIL cannot enter assets directory " << assets << '\n';
        return 2;
    }
    nl::nx::load_all();

    bool ok = true;

    // 0. Real nx samples.
    const std::pair<int32_t, int32_t> caps = find_sample_caps();
    const int32_t cash_hat = caps.first;
    const int32_t normal_hat = caps.second;
    ok &= expect(cash_hat != 0, "cash cap sample found in Character.nx");
    ok &= expect(normal_hat != 0, "normal cap sample found in Character.nx");
    std::cout << "samples: cash hat " << cash_hat
        << ", normal hat " << normal_hat << '\n';

    // 1. Inventory categorization by item id.
    ok &= expect(InventoryType::by_item_id(cash_hat) == InventoryType::CASH,
        "cash cap sorts into CASH inventory");
    ok &= expect(InventoryType::by_item_id(normal_hat) == InventoryType::EQUIP,
        "normal cap sorts into EQUIP inventory");
    ok &= expect(InventoryType::by_item_id(2000000) == InventoryType::USE,
        "consumable still sorts into USE inventory");

    // 2. CASH-tab packet entry parses with the equip format.
    Inventory inv;
    Bytes bytes;
    append_equip_entry(bytes, cash_hat, true, 0x11223344, 30);
    InPacket packet(bytes.data(), bytes.size());
    ItemParser::parse_item(packet, InventoryType::CASH, 3, inv);
    ok &= expect(inv.get_item_id(InventoryType::CASH, 3) == cash_hat,
        "cash equip entry lands in CASH slot 3");
    ok &= expect(inv.is_cash_item(InventoryType::CASH, 3),
        "cash equip entry flagged as cash item");
    ok &= expect(inv.get_cash_id(InventoryType::CASH, 3) == 0x11223344,
        "cash equip entry keeps its cash id");

    // 3. Wear it: server MOVE_EQUIP to hidden slot 101 (CAP + 100).
    inv.modify(InventoryType::CASH, 3, Inventory::SWAP, -101,
        Inventory::MOVE_EQUIP);
    ok &= expect(inv.get_item_id(InventoryType::EQUIP, 101) == cash_hat,
        "worn cash equip moves to hidden EQUIP slot 101");
    ok &= expect(inv.get_item_id(InventoryType::CASH, 3) == 0,
        "CASH slot 3 emptied by the move");
    ok &= expect(!inv.has_equipped(Equipslot::CAP),
        "worn cash equip does not enter EQUIPPED");

    // 4. Stats isolation: the cash equip's STR +30 stays out of the totals.
    inv.recalc_stats(Weapon::NONE);
    ok &= expect(inv.get_stat(Equipstat::STR) == 0,
        "cash equip stats excluded from totals");
    ok &= expect(inv.has_equipped_item(cash_hat),
        "worn cash equip still reported as equipped");

    // 5. Control group: a normal cap contributes stats normally.
    Bytes normal_bytes;
    append_equip_entry(normal_bytes, normal_hat, false, 0, 5);
    InPacket normal_packet(normal_bytes.data(), normal_bytes.size());
    ItemParser::parse_item(normal_packet, InventoryType::EQUIP, 1, inv);
    ok &= expect(inv.get_item_id(InventoryType::EQUIP, 1) == normal_hat,
        "normal cap entry lands in EQUIP slot 1");
    inv.modify(InventoryType::EQUIP, 1, Inventory::SWAP, -1,
        Inventory::MOVE_EQUIP);
    ok &= expect(inv.has_equipped(Equipslot::CAP),
        "normal cap enters EQUIPPED cap slot");
    inv.recalc_stats(Weapon::NONE);
    ok &= expect(inv.get_stat(Equipstat::STR) == 5,
        "normal cap stats included in totals");
    ok &= expect(inv.get_item_id(InventoryType::EQUIP, 101) == cash_hat,
        "hidden cash equip slot untouched by normal equip");

    // 6. Take the cash equip off: it returns to the CASH tab.
    inv.modify(InventoryType::EQUIP, 101, Inventory::SWAP, 7,
        Inventory::MOVE_UNEQUIP);
    ok &= expect(inv.get_item_id(InventoryType::CASH, 7) == cash_hat,
        "unequipped cash cap returns to CASH slot 7");
    ok &= expect(inv.get_item_id(InventoryType::EQUIP, 101) == 0,
        "hidden EQUIP slot 101 emptied");
    inv.recalc_stats(Weapon::NONE);
    ok &= expect(inv.get_stat(Equipstat::STR) == 5,
        "normal cap stats kept after cash unequip");

    // 7. SET_FIELD cash-equipped segment: slot -101 parses into EQUIP 101.
    Inventory inv2;
    Bytes setfield_bytes;
    append_equip_entry(setfield_bytes, cash_hat, true, 0x99, 77);
    InPacket setfield_packet(setfield_bytes.data(), setfield_bytes.size());
    ItemParser::parse_item(setfield_packet, InventoryType::EQUIP, -101, inv2);
    ok &= expect(inv2.get_item_id(InventoryType::EQUIP, 101) == cash_hat,
        "set-field cash equip parses into hidden EQUIP slot 101");
    inv2.recalc_stats(Weapon::NONE);
    ok &= expect(inv2.get_stat(Equipstat::STR) == 0,
        "set-field cash equip stats excluded from totals");

    // 8. Cash weapon covers are cosmetic: the EQUIPPED weapon slot keeps the
    // real weapon (the id get_weaponid()/get_weapontype() derive gameplay
    // data from), while the worn cover lives only in the hidden EQUIP slot
    // 100 + WEAPON and its stats never feed the totals.
    const std::pair<int32_t, int32_t> weapons = find_sample_weapons();
    const int32_t real_weapon = weapons.first;
    const int32_t cash_cover = weapons.second;
    ok &= expect(real_weapon != 0, "real weapon sample found in Character.nx");
    ok &= expect(cash_cover != 0,
        "cash weapon cover sample found in Character.nx");
    std::cout << "samples: real weapon " << real_weapon
        << ", cash weapon cover " << cash_cover << '\n';
    const Weapon::Type real_type = Weapon::by_value(real_weapon / 10000);
    ok &= expect(real_type != Weapon::NONE && real_type != Weapon::CASH,
        "real weapon id maps to a gameplay weapon type");
    ok &= expect(Weapon::by_value(cash_cover / 10000) == Weapon::CASH,
        "cover id alone maps to Weapon::CASH, not a gameplay weapon type");

    Inventory inv3;
    Bytes weapon_bytes;
    append_equip_entry(weapon_bytes, real_weapon, false, 0, 0, 7);
    InPacket weapon_packet(weapon_bytes.data(), weapon_bytes.size());
    ItemParser::parse_item(weapon_packet, InventoryType::EQUIPPED, -11, inv3);
    ok &= expect(inv3.get_item_id(InventoryType::EQUIPPED, Equipslot::WEAPON)
        == real_weapon, "real weapon parses into EQUIPPED weapon slot");

    Bytes cover_bytes;
    append_equip_entry(cover_bytes, cash_cover, true, 0x55667788, 0, 88);
    InPacket cover_packet(cover_bytes.data(), cover_bytes.size());
    ItemParser::parse_item(cover_packet, InventoryType::EQUIPPED, -111, inv3);
    ok &= expect(inv3.get_item_id(InventoryType::EQUIP,
        100 + Equipslot::WEAPON) == cash_cover,
        "cash weapon cover parses into hidden EQUIP slot 111");
    ok &= expect(!inv3.has_equipped(Equipslot::WEAPON)
        || inv3.get_item_id(InventoryType::EQUIPPED, Equipslot::WEAPON)
            == real_weapon,
        "EQUIPPED weapon slot still carries the real weapon id");

    // The real weapon's WATK 7 is counted; no bullet WATK can be added since
    // inv3 has no USE items (and the BulletData stub reports 0 anyway), so
    // the cover's WATK 88 must not move the total off 7.
    inv3.recalc_stats(real_type);
    ok &= expect(inv3.get_stat(Equipstat::WATK) >= 7,
        "real weapon WATK counted in totals");
    ok &= expect(inv3.get_stat(Equipstat::WATK) == 7,
        "cash weapon cover WATK 88 excluded from totals");

    if (ok)
        std::cout << "PASS cash equip policy\n";
    return ok ? 0 : 1;
}
