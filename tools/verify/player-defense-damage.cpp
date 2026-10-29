// End-to-end audit of the player-side defense settlement chain:
//   server addItemInfo bytes -> ItemParser::parse_item -> Inventory::add_equip
//   -> Inventory::recalc_stats (EQUIPPED accumulation) -> Player::recalc_stats
//   merge -> CharStats::calculate_damage (mobatk / WDEF / MDEF).
//
// Reproduces three real flows with the production sources linked in:
//   1. login with the armor already worn (ADD at slot -5),
//   2. equipping mid-game (ADD to EQUIP then SWAP with MOVE_EQUIP),
//   3. unequipping mid-game (SWAP with MOVE_UNEQUIP).
// After each flow the WDEF/MDEF totals and the resulting damage taken from a
// fixed mob attack must change in the expected direction; wearing armor must
// reduce incoming damage and taking it off must restore the naked value.
// usage: player-defense-damage
#include "Character/CharStats.h"
#include "Character/PassiveBuffs.h"
#include "Character/Inventory/EquipQuality.h"
#include "Data/BulletData.h"
#include "Data/ItemData.h"
#include "Graphics/Texture.h"
#include "Character/Inventory/Inventory.h"
#include "Net/Handlers/Helpers/ItemParser.h"
#include "Net/InPacket.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    // Backing storage for the never-constructed dummy ItemData reference.
    alignas(16) unsigned char dummy_item_storage[64];
    using Bytes = std::vector<int8_t>;

    void append_number(Bytes& bytes, uint64_t value, size_t width)
    {
        for (size_t i = 0; i < width; ++i)
        {
            bytes.push_back(static_cast<int8_t>(value >> (i * 8)));
        }
    }

    void append_string(Bytes& bytes, const std::string& value)
    {
        append_number(bytes, value.size(), 2);
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    // Mirror of MaplePacketCreator.addItemInfo for a non-cash equip, starting
    // after the position field (the caller supplies the slot separately).
    Bytes equip_body(int32_t item_id, uint16_t wdef, uint16_t mdef,
        uint16_t watk = 0)
    {
        Bytes bytes;
        append_number(bytes, 1, 1);          // wire item type: equip
        append_number(bytes, static_cast<uint64_t>(item_id), 4);
        append_number(bytes, 0, 1);          // isCash = false
        append_number(bytes, 0, 8);          // expiration
        append_number(bytes, 7, 1);          // upgrade slots
        append_number(bytes, 0, 1);          // scroll level
        const uint16_t stat_line[15] = {
            0, 0, 0, 0, 0, 0,               // STR DEX INT LUK HP MP
            watk, 0, wdef, mdef, 0,        // WATK MAGIC WDEF MDEF ACC
            0, 0, 0, 0                      // AVOID HANDS SPEED JUMP
        };
        for (uint16_t stat : stat_line)
        {
            append_number(bytes, stat, 2);
        }
        append_string(bytes, "");            // owner
        append_number(bytes, 0, 2);          // item flags
        append_number(bytes, 0, 1);          // padding byte
        append_number(bytes, 0, 1);          // item level
        append_number(bytes, 0, 2);          // exp nibble hi
        append_number(bytes, 0, 2);          // exp nibble lo
        append_number(bytes, 0, 4);          // vicious
        append_number(bytes, 0, 8);          // reserved long
        append_number(bytes, 0, 12);         // timestamp long + trailing int
        return bytes;
    }

    jrc::StatsEntry base_stats_entry()
    {
        jrc::StatsEntry entry;
        entry.name = "Verify";
        entry.stats[jrc::Maplestat::LEVEL] = 50;
        entry.stats[jrc::Maplestat::JOB] = 100;    // beginner branch
        entry.stats[jrc::Maplestat::STR] = 100;
        entry.stats[jrc::Maplestat::DEX] = 25;
        entry.stats[jrc::Maplestat::INT] = 4;
        entry.stats[jrc::Maplestat::LUK] = 4;
        entry.stats[jrc::Maplestat::MAXHP] = 2000;
        entry.stats[jrc::Maplestat::MAXMP] = 500;
        return entry;
    }

    // Same merge Player::recalc_stats performs after Inventory::recalc_stats.
    jrc::CharStats merged_stats(const jrc::Inventory& inventory)
    {
        jrc::CharStats stats(base_stats_entry());
        stats.set_weapontype(jrc::Weapon::NONE);
        stats.init_totalstats();
        for (auto stat : jrc::Equipstat::values)
        {
            stats.add_value(stat, inventory.get_stat(stat));
        }
        stats.close_totalstats();
        return stats;
    }

    void parse_equipped(jrc::Inventory& inventory, int16_t slot)
    {
        const Bytes body = equip_body(1040036, 50, 30);
        jrc::InPacket recv(body.data(), body.size());
        jrc::ItemParser::parse_item(
            recv, jrc::InventoryType::EQUIPPED, slot, inventory);
    }
}

namespace jrc
{
    // Stubs for NX-backed collaborators that are not part of the defense
    // settlement under audit. The bullet path only runs for projectile
    // weapons (none in these flows), so the raw storage never backs a live
    // object.
    BulletData::BulletData(int32_t)
        : itemdata(*reinterpret_cast<const ItemData*>(
              dummy_item_storage)), watk(0) {}

    int16_t BulletData::get_watk() const
    {
        return watk;
    }

    Texture::~Texture() {}

    Animation::Animation() {}

    EquipQuality::Id EquipQuality::check_quality(int32_t, bool,
        const EnumMap<Equipstat::Id, uint16_t>&)
    {
        return EquipQuality::WHITE;
    }
}

int main()
{
    namespace es = jrc::Equipstat;
    constexpr int32_t MOB_ATK = 300;

    // Flow 1: login with the armor already worn.
    jrc::Inventory inventory;
    parse_equipped(inventory, -5);
    inventory.recalc_stats(jrc::Weapon::NONE);
    const int32_t worn_wdef = inventory.get_stat(es::WDEF);
    const int32_t worn_mdef = inventory.get_stat(es::MDEF);
    jrc::CharStats armored = merged_stats(inventory);

    jrc::Inventory naked_inventory;
    naked_inventory.recalc_stats(jrc::Weapon::NONE);
    jrc::CharStats naked = merged_stats(naked_inventory);

    std::cout << "login flow: worn WDEF=" << worn_wdef
              << " MDEF=" << worn_mdef << '\n';
    std::cout << "physical damage: armored="
              << armored.calculate_damage(MOB_ATK)
              << " naked=" << naked.calculate_damage(MOB_ATK) << '\n';
    std::cout << "magic damage:    armored="
              << armored.calculate_damage(MOB_ATK, true)
              << " naked=" << naked.calculate_damage(MOB_ATK, true) << '\n';

    assert(worn_wdef == 50);
    assert(worn_mdef == 30);
    assert(naked.get_total(es::WDEF) == 0);
    assert(armored.get_total(es::WDEF) == 50);
    assert(armored.calculate_damage(MOB_ATK)
        < naked.calculate_damage(MOB_ATK));
    assert(armored.calculate_damage(MOB_ATK, true)
        < naked.calculate_damage(MOB_ATK, true));

    // Flow 2: equip mid-game (item arrives in EQUIP, then SWAP MOVE_EQUIP).
    jrc::Inventory midgame;
    {
        const Bytes body = equip_body(1040036, 50, 30);
        jrc::InPacket recv(body.data(), body.size());
        jrc::ItemParser::parse_item(
            recv, jrc::InventoryType::EQUIP, 1, midgame);
    }
    midgame.recalc_stats(jrc::Weapon::NONE);
    jrc::CharStats before = merged_stats(midgame);
    midgame.modify(jrc::InventoryType::EQUIP, 1, jrc::Inventory::SWAP, -5,
        jrc::Inventory::MOVE_EQUIP);
    midgame.recalc_stats(jrc::Weapon::NONE);
    jrc::CharStats after = merged_stats(midgame);

    std::cout << "midgame equip: before="
              << before.calculate_damage(MOB_ATK)
              << " after=" << after.calculate_damage(MOB_ATK) << '\n';
    assert(before.calculate_damage(MOB_ATK)
        == naked.calculate_damage(MOB_ATK));
    assert(after.get_total(es::WDEF) == 50);
    assert(after.calculate_damage(MOB_ATK)
        == armored.calculate_damage(MOB_ATK));

    // Flow 3: unequip mid-game (SWAP MOVE_UNEQUIP back to EQUIP).
    midgame.modify(jrc::InventoryType::EQUIPPED, 5, jrc::Inventory::SWAP, 1,
        jrc::Inventory::MOVE_UNEQUIP);
    midgame.recalc_stats(jrc::Weapon::NONE);
    jrc::CharStats stripped = merged_stats(midgame);

    std::cout << "midgame unequip: damage="
              << stripped.calculate_damage(MOB_ATK) << '\n';
    assert(stripped.get_total(es::WDEF) == 0);
    assert(stripped.calculate_damage(MOB_ATK)
        == naked.calculate_damage(MOB_ATK));

    // Flow 4: a weapon's WATK must widen the outgoing damage range.
    jrc::Inventory armed_inventory;
    {
        const Bytes body = equip_body(1302008, 0, 0, 30);
        jrc::InPacket recv(body.data(), body.size());
        jrc::ItemParser::parse_item(
            recv, jrc::InventoryType::EQUIPPED, -11, armed_inventory);
    }
    armed_inventory.recalc_stats(jrc::Weapon::NONE);
    jrc::CharStats armed_weapon = merged_stats(armed_inventory);

    std::cout << "weapon flow: maxdamage unarmed="
              << naked.get_maxdamage()
              << " armed=" << armed_weapon.get_maxdamage() << '\n';
    assert(armed_inventory.get_stat(es::WATK) == 30);
    assert(armed_weapon.get_maxdamage() > naked.get_maxdamage());

    // Flow 5: Achilles reduction. WZ stores the retained share in per-mille
    // (Lv1 x = 995, Lv30 x = 800), so the reduction is (1000 - x) / 1000,
    // clamped into [0, 1] with a missing node (0) meaning no reduction.
    // The old code used x / 1000 and cut 80% of every hit instead of 20%.
    const auto about = [](float a, float b)
    {
        return (a - b < 0.0f ? b - a : a - b) < 1e-6f;
    };
    assert(about(jrc::achilles_reduction(800), 0.2f));
    assert(about(jrc::achilles_reduction(995), 0.005f));
    assert(jrc::achilles_reduction(0) == 0.0f);
    assert(jrc::achilles_reduction(1200) == 0.0f);

    jrc::CharStats shielded = armored;
    shielded.set_reducedamage(jrc::achilles_reduction(800));
    // WDEF=50: v83 系数 0.7 抵扣全额怪攻后 base = max(1, (1000-35)/2) = 482。
    const int32_t shield_base = (1000 - 50 * 7 / 10) / 2;
    std::cout << "achilles flow: damage="
              << shielded.calculate_damage(1000)
              << " (base " << shield_base << " -20%)\n";
    assert(shielded.calculate_damage(1000)
        == shield_base - static_cast<int32_t>(shield_base * 0.2f));
    assert(shielded.calculate_damage(1000)
        < armored.calculate_damage(1000));

    std::cout << "player defense damage verification passed\n";
    return 0;
}
