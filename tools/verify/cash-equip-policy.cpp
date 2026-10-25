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
//     negative -100-ish slots which the parser keeps in hidden EQUIP slots.
//
// Real Character.nx data is used: the first cash cap and the first non-cash
// cap found under character/Cap drive the fake packets.
//
// Usage (from the repository root; clang++ is required because GCC rejects
// Cache<ItemData>'s unordered_map member while ItemData is still incomplete,
// while the WASM build's emscripten/clang frontend defers it):
//   clang++ -std=c++17 -Wno-c++11-narrowing -Isrc/client -Isrc \
//       -Isrc/client/includes tools/verify/cash-equip-policy.cpp \
//       src/client/Character/Inventory/Inventory.cpp \
//       src/client/Character/Inventory/InventoryType.cpp \
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
        int64_t cash_id, uint16_t str_inc)
    {
        append_byte(bytes, 1);
        append_int(bytes, itemid);
        append_byte(bytes, cash ? 1 : 0);
        if (cash)
            append_long(bytes, cash_id);
        append_long(bytes, 0);
        append_byte(bytes, 0);
        append_byte(bytes, 0);
        append_short(bytes, static_cast<int16_t>(str_inc));
        for (int i = 1; i < jrc::Equipstat::LENGTH; ++i)
            append_short(bytes, 0);
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

    if (ok)
        std::cout << "PASS cash equip policy\n";
    return ok ? 0 : 1;
}
