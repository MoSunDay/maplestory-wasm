#include "../Stage.h"

#include "../../Character/Buff.h"
#include "../../Character/Inventory/Weapon.h"
#include "../../Data/ItemData.h"
#include "../../Data/SkillData.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIStatusBar.h"
#include "../../Net/Packets/AttackAndSkillPackets.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/PetPackets.h"
#include "PetBuffPolicy.h"
#include "PetLootPolicy.h"
#include "PetSettingsStore.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace jrc
{
    namespace
    {
        constexpr uint16_t PET_LOOT_COOLDOWN_FRAMES = 45;

        int32_t current_party_id()
        {
            if (Optional<UIStatusbar> statusbar = UI::get().get_element<UIStatusbar>())
                return statusbar->get_party_id();
            return -1;
        }
    }

    void Stage::set_pet_buff_skill(size_t slot, int32_t skill_id)
    {
        if (slot >= pet_automation::MAX_BUFF_SKILLS)
            return;

        pet_automation::BuffSkillSlots next = pet_automation.get_buff_skills();
        if (skill_id > 0)
        {
            // Preserve the historical move semantics for a single slot.
            for (int32_t& id : next)
            {
                if (id == skill_id)
                    id = 0;
            }
            next[slot] = skill_id;
        }
        else
        {
            next[slot] = 0;
        }
        apply_pet_buff_skills(next);
    }

    bool Stage::apply_pet_buff_skills(const pet_automation::BuffSkillSlots& skills)
    {
        pet_automation::BuffSkillSlots sanitized{};
        for (size_t slot = 0; slot < skills.size(); ++slot)
        {
            const int32_t skill_id = skills[slot];
            if (skill_id <= 0)
                continue;
            if (std::find(sanitized.begin(), sanitized.end(), skill_id) !=
                sanitized.end())
                continue;

            const SkillData& skill = SkillData::get(skill_id);
            const int32_t level = player.get_skilllevel(skill_id);
            if (!pet_buff::skill_can_auto_buff(skill, skill_id, level))
                continue;

            sanitized[slot] = skill_id;
        }

        const pet_automation::BuffSkillSlots previous =
            pet_automation.get_buff_skills();
        pet_automation.set_buff_skills(sanitized);
        if (!pet_settings::save_buff_skills(sanitized))
        {
            pet_automation.set_buff_skills(previous);
            return false;
        }
        return true;
    }

    void Stage::confirm_pet_buff(int32_t skill_id)
    {
        pet_automation.confirm_buff(skill_id);
    }

    void Stage::update_pet_buffs()
    {
        PetLook* pet = player.get_pet(0);
        if (!pet || player.is_dead())
            return;
        if (pet_automation.has_pending_buff())
            return;

        const bool pet_active = pet->get_itemid() != 0;
        const bool alive = !player.is_dead();
        const bool incapacitated = player.has_buff(Buffstat::STUN) ||
            player.has_buff(Buffstat::SEDUCE);
        const pet_automation::BuffSkillSlots& skills =
            pet_automation.get_buff_skills();

        for (size_t slot = 0; slot < skills.size(); ++slot)
        {
            const int32_t skill_id = skills[slot];
            if (skill_id <= 0)
                continue;

            const SkillData& skill = SkillData::get(skill_id);
            const int32_t level = player.get_skilllevel(skill_id);
            const SkillData::Stats& stats = skill.get_stats(level);
            const bool allowed = pet_buff::skill_can_auto_buff(
                skill, skill_id, level);
            const bool skill_ready = allowed && !player.has_cooldown(skill_id);
            const bool mp_ok = player.get_stats().get_stat(Maplestat::MP) >=
                stats.mpcost;
            // The weapon gate lives on the cast path only: persisted slots
            // must survive a temporary weapon swap in apply_pet_buff_skills.
            const Weapon::Type required = skill.get_required_weapon();
            const bool weapon_ok = required == Weapon::NONE ||
                required == player.get_weapontype();
            const bool castable = allowed && !incapacitated && weapon_ok;

            const bool buff_active = pet_buff::buff_slot_satisfied(
                skill_id,
                player.has_skill_buff(skill_id),
                player.has_buff(Buffstat::BOOSTER),
                player.has_buff(Buffstat::WK_CHARGE));

            const bool cast = pet_automation.decide_buff(slot, {
                pet_active,
                alive,
                buff_active,
                skill_ready,
                mp_ok,
                castable
            });
            if (cast)
                UseSkillPacket(skill_id, level).dispatch();
        }
    }

    void Stage::update_pet_automation()
    {
        pet_automation.tick();
        if (pet_loot_cooldown > 0)
            --pet_loot_cooldown;
        if (pet_loot_blocked_oid &&
            !drops.has_lootable_drop(pet_loot_blocked_oid))
        {
            pet_loot_blocked_oid = 0;
        }

        PetLook* pet = player.get_pet(0);
        if (!pet)
        {
            clear_pet_loot_target(nullptr);
            return;
        }
        const bool pet_active = pet->get_itemid() != 0;

        const Inventory& inventory = player.get_inventory();
        const bool hungry = is_pet_hungry();

        // Feed the pet before the hunger strike check so a hungry pet is
        // topped up automatically instead of staying on strike forever.
        const int32_t food_item = pet_automation.get_food_item();
        const int16_t food_slot = inventory.find_item(InventoryType::USE, food_item);
        const int32_t feed_item = pet_automation.decide_feed({
            pet_active, hungry, food_slot > 0
        });
        if (feed_item > 0)
            PetFoodPacket(food_slot, feed_item).dispatch();

        // Buffs still run while hungry; only looting and potions pause.
        update_pet_buffs();

        if (hungry)
        {
            clear_pet_loot_target(pet);
            return;
        }

        const pet_capability::Capabilities capabilities = get_pet_capabilities();
        const CharStats& stats = player.get_stats();
        int16_t hp_slot = inventory.find_item(InventoryType::USE, pet_automation.get_hp_item());
        int16_t mp_slot = inventory.find_item(InventoryType::USE, pet_automation.get_mp_item());
        const ItemData& hp_item = ItemData::get(pet_automation.get_hp_item());
        const ItemData& mp_item = ItemData::get(pet_automation.get_mp_item());
        jrc::pet_automation::AutoPotInputs inputs = {
            stats.get_stat(Maplestat::HP),
            stats.get_stat(Maplestat::MAXHP),
            stats.get_stat(Maplestat::MP),
            stats.get_stat(Maplestat::MAXMP),
            stats.get_stat(Maplestat::HP) > 0,
            pet_active,
            capabilities.auto_hp_pouch,
            capabilities.auto_mp_pouch,
            hp_slot > 0 && hp_item && hp_item.restores_hp(),
            mp_slot > 0 && mp_item && mp_item.restores_mp()
        };

        jrc::pet_automation::AutoPotDecisions decisions = pet_automation.decide(inputs);
        for (size_t i = 0; i < decisions.count; ++i)
        {
            int32_t item_id = decisions.requests[i].item_id;
            int16_t slot = inventory.find_item(InventoryType::USE, item_id);
            if (slot > 0)
                PetAutoPotPacket(0, pet->get_uniqueid(), slot, item_id).dispatch();
        }

        if (!capabilities.wing_boots)
        {
            clear_pet_loot_target(pet);
            return;
        }

        const pet_follow::MovementProfile pet_movement = {
            player.get_walkforce(), player.get_jumpforce()
        };
        MapDrops::PetLootQuery query = {
            player.get_position(),
            player.get_oid(),
            current_party_id(),
            600,
            capabilities.meso_magnet,
            capabilities.item_pouch,
            capabilities.magic_scales,
            pet_follow::pursuit_height_limit(pet_movement),
            pet_loot_blocked_oid
        };

        if (pet_loot_target_oid)
        {
            // Drops keep falling and scattering after spawn; track the live
            // icon center so the pet never parks at a stale capture.
            const std::optional<Point<int16_t>> live_position =
                drops.pet_loot_position(pet_loot_target_oid);
            if (!live_position)
            {
                clear_pet_loot_target(pet);
            }
            else if (*live_position != pet_loot_target_position)
            {
                pet_loot_target_position = *live_position;
                pet->set_loot_target(*live_position);
            }
        }

        if (pet_loot_target_oid && !pet->has_loot_target())
        {
            // The pet gave the target up (out of the owner's reach); cool down
            // and park the oid so the unreachable drop is not instantly
            // re-targeted.
            pet_loot_blocked_oid = pet_loot_target_oid;
            clear_pet_loot_target(pet);
            pet_loot_cooldown = PET_LOOT_COOLDOWN_FRAMES;
        }

        if (!pet_loot_target_oid && pet_loot_cooldown == 0)
        {
            MapDrops::PetLootTarget target = drops.find_pet_loot(query);
            if (target.oid)
            {
                pet_loot_target_oid = target.oid;
                pet_loot_target_position = target.position;
                pet_loot_target_frames = 0;
                pet_loot_attempts = 0;
                pet->set_loot_target(target.position);
            }
        }

        if (!pet_loot_target_oid)
            return;

        ++pet_loot_target_frames;
        bool pet_in_range = pet_loot::in_reach(pet->get_position(),
            pet_loot_target_position, query.max_height_above);
        if (pet_loot::should_attempt(pet_loot_target_frames,
            pet_loot_attempts, pet_in_range))
        {
            // One sweep picks the whole cluster around the pet, nearest first.
            const std::vector<int32_t> batch = drops.find_pet_loot_batch(
                query, pet->get_position());
            const uint8_t count = pet_loot::batch_count(batch.size());
            for (uint8_t i = 0; i < count; ++i)
                PetLootPacket(pet->get_uniqueid(), batch[i]).dispatch();
            ++pet_loot_attempts;
            if (pet_loot::attempts_exhausted(pet_loot_attempts))
            {
                clear_pet_loot_target(pet);
                pet_loot_cooldown = PET_LOOT_COOLDOWN_FRAMES;
            }
        }

        if (pet_loot::should_abandon(pet_loot_target_frames, pet_in_range))
        {
            // A walk-only pursuit that has not closed the gap by now (deep
            // pit, unreachable ledge) never will; give the drop up and park
            // the oid so the pet resumes following instead of standing
            // frozen beside loot it cannot pick up.
            pet_loot_blocked_oid = pet_loot_target_oid;
            clear_pet_loot_target(pet);
            pet_loot_cooldown = PET_LOOT_COOLDOWN_FRAMES;
        }
    }
}
