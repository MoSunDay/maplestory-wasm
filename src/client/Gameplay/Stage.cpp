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
#include "Stage.h"

#include "../Data/ItemData.h"
#include "../IO/Messages.h"
#include "../IO/UI.h"
#include "../IO/UITypes/UIStatusBar.h"
#include "../Graphics/GraphicsGL.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Net/Packets/AttackAndSkillPackets.h"
#include "../Net/Packets/PetPackets.h"
#include "../Util/Misc.h"
#include "Pet/PetSettingsStore.h"
#include "Pet/PetLootPolicy.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        constexpr uint16_t PET_LOOT_COOLDOWN_FRAMES = 45;
        constexpr uint8_t PET_MOVEMENT_SYNC_FRAMES = 6;

        int32_t current_party_id()
        {
            if (Optional<UIStatusbar> statusbar = UI::get().get_element<UIStatusbar>())
                return statusbar->get_party_id();
            return -1;
        }
    }

    Stage::Stage()
        : combat(player, chars, mobs, physics)
        , last_pickup_time(0)
        , pet_automation()
        , pet_loot_target_oid(0)
        , pet_loot_target_frames(0)
        , pet_loot_cooldown(0)
        , pet_loot_attempts(0)
        , pet_movement_sync_frames(0)
        , pending_intro_warp_mapid(-1)
        , pending_intro_warp_delay_ms(0)
        , asset_generation(0)
    {
        state = INACTIVE;
        mapid = 0;
        pet_settings::Thresholds thresholds = pet_settings::load();
        pet_automation.set_hp_threshold(thresholds.hp);
        pet_automation.set_mp_threshold(thresholds.mp);
    }

    void Stage::init()
    {
        drops.init();
    }

    void Stage::load(int32_t mapid, int8_t portalid, std::function<void()> onready)
    {
        begin_loading(std::move(onready));
        player.prepare_attack_assets();
        load_map(mapid);
        respawn(portalid);
        report_loading_progress();
    }

    void Stage::loadplayer(const CharEntry& entry)
    {
        player = entry;
        playable = player;
    }

    void Stage::clear()
    {
        end_loading();
        state = INACTIVE;
        mapid = 0;
        effect = MapEffect();
        pending_intro_warp_mapid = -1;
        pending_intro_warp_delay_ms = 0;
        combat.clear();
        chars.clear();
        npcs.clear();
        mobs.clear();
        drops.clear();
        mists.clear();
        reactors.clear();
        pet_loot_target_oid = 0;
        pet_loot_target_frames = 0;
        pet_loot_cooldown = 0;
        pet_loot_attempts = 0;
        pet_movement_sync_frames = 0;
        last_pet_movement = Movement();
    }

    void Stage::load_map(int32_t mapid)
    {
        this->mapid = mapid;

        std::string strid  = string_format::extend_id(mapid, 9);
        std::string prefix = std::to_string(mapid / 100000000);
        nl::node src       = nl::nx::map["Map"]["Map" + prefix][strid + ".img"];

        tilesobjs   = MapTilesObjs(src);
        backgrounds = MapBackgrounds(src["back"]);
        physics     = Physics(src["foothold"]);
        mapinfo     = MapInfo(src, physics.get_fht().get_walls(), physics.get_fht().get_borders());
        // Audio data is part of the map safety set. Web Audio decoding remains
        // asynchronous, but the NX range itself is resident before gameplay.
        Music(mapinfo.get_bgm()).prepare();
        player.set_recovery_rate(mapinfo.get_recovery_rate());
        portals     = MapPortals(src["portal"], mapid);
        // Keep any effect injected during the fade transition (e.g. intro Scene packets).
        // `clear()` already resets stale effects before starting a map change.
    }

    void Stage::respawn(int8_t portalid)
    {
        Point<int16_t> spawnpoint = portals.get_portal_by_id(static_cast<uint8_t>(portalid));
        Point<int16_t> startpos   = physics.get_y_below(spawnpoint);

        player.respawn(startpos, mapinfo.is_underwater());
        camera.set_position(startpos);
        camera.set_view(mapinfo.get_walls(), mapinfo.get_borders());
    }

    void Stage::draw(float alpha) const
    {
        if (state == INACTIVE)
        {
            return;
        }

        Point<int16_t> viewpos = camera.position(alpha);
        Point<double> viewrpos = camera.realposition(alpha);
        double viewx = viewrpos.x();
        double viewy = viewrpos.y();

        if (state == LOADING)
        {
            // Promote every frame of visible static animations before the
            // transition can release; network prefetch still covers the rest.
            backgrounds.prepare_visible(viewx, viewy, alpha);
            tilesobjs.prepare_visible(viewpos, alpha);
        }

        backgrounds.drawbackgrounds(viewx, viewy, alpha);
        for (auto id : Layer::IDs)
        {
            tilesobjs.draw(id, viewpos, alpha);
            if (id == Layer::ONE)
            {
                mists.draw(viewx, viewy);
            }
            reactors.draw(id, viewx, viewy, alpha);
            npcs.draw(id, viewx, viewy, alpha);
            mobs.draw(id, viewx, viewy, alpha);
            chars.draw(id, viewx, viewy, alpha);
            player.draw(id, viewx, viewy, alpha);
            drops.draw(id, viewx, viewy, alpha);
        }
        combat.draw(viewx, viewy, alpha);
        portals.draw(viewpos, alpha);
        backgrounds.drawforegrounds(viewx, viewy, alpha);
        effect.draw();

        if (player.has_buff(Buffstat::FEAR))
        {
            const int16_t width = Constants::viewwidth();
            const int16_t height = Constants::viewheight();
            const int16_t center_x = static_cast<int16_t>(player.get_position().x() + viewx);
            const int16_t center_y = static_cast<int16_t>(player.get_position().y() + viewy);
            const int16_t left = std::clamp<int16_t>(center_x - 200, 0, width);
            const int16_t right = std::clamp<int16_t>(center_x + 200, 0, width);
            const int16_t top = std::clamp<int16_t>(center_y - 200, 0, height);
            const int16_t bottom = std::clamp<int16_t>(center_y + 200, 0, height);
            GraphicsGL& graphics = GraphicsGL::get();
            graphics.drawrectangle(0, -Constants::VIEWYOFFSET, width, top, 0, 0, 0, 0.92f);
            graphics.drawrectangle(0, top, left, bottom - top, 0, 0, 0, 0.92f);
            graphics.drawrectangle(right, top, width - right, bottom - top, 0, 0, 0, 0.92f);
            graphics.drawrectangle(0, bottom, width, height - bottom, 0, 0, 0, 0.92f);
        }
    }

    void Stage::update()
    {
        if (state == LOADING)
        {
            update_loading();
            return;
        }
        if (state != ACTIVE)
        {
            return;
        }

        combat.update();
        backgrounds.update();
        effect.update();
        tilesobjs.update();

        reactors.update(physics);
        npcs.update(physics);
        mobs.update(physics, player.get_position());
        chars.update(physics);
        drops.update(physics);
        player.update(physics);
        update_pet_movement();
        update_pet_automation();
        if (is_intro_input_locked())
        {
            // Direction3-style intro scenes present the actor facing left and
            // should not react to local movement intent while the scene runs.
            player.set_direction(false);
        }
        update_intro_warp();
        handle_held_actions();
        update_directional_context();

        portals.update(player.get_position());
        camera.update(player.get_position());

        MobAttack action_attack = mobs.poll_action_attack(player.get_position());
        if (player.is_invincible())
        {
            return;
        }

        if (action_attack)
        {
            MobAttackResult result = player.damage(action_attack);
            TakeDamagePacket(result, result.attack_index).dispatch();
            return;
        }

        if (int32_t oid_id = mobs.find_colliding(player.get_phobj()))
        {
            if (MobAttack attack = mobs.create_attack(oid_id))
            {
                MobAttackResult result = player.damage(attack);
                TakeDamagePacket(result, TakeDamagePacket::TOUCH).dispatch();
            }
        }
    }

    void Stage::show_character_effect(int32_t cid, CharEffect::Id effect)
    {
        if (auto character = get_character(cid))
        {
            character->show_effect_id(effect);
        }
    }

    size_t Stage::set_named_object_active(const std::string& name, bool active)
    {
        return tilesobjs.set_named_object_active(name, active);
    }

    size_t Stage::count_active_named_objects(const std::string& name) const
    {
        return tilesobjs.count_active_named_objects(name);
    }

    void Stage::clear_pet_loot_target(PetLook* pet)
    {
        if (pet)
            pet->clear_loot_target();
        pet_loot_target_oid = 0;
        pet_loot_target_frames = 0;
        pet_loot_attempts = 0;
    }

    void Stage::update_pet_automation()
    {
        pet_automation.tick();
        if (pet_loot_cooldown > 0)
            --pet_loot_cooldown;

        PetLook* pet = player.get_pet(0);
        if (!pet)
        {
            clear_pet_loot_target(nullptr);
            return;
        }

        const Inventory& inventory = player.get_inventory();
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
            true,
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

        if (pet_loot_target_oid && !drops.has_lootable_drop(pet_loot_target_oid))
        {
            clear_pet_loot_target(pet);
        }

        if (pet_loot_target_oid && !pet->has_loot_target())
        {
            clear_pet_loot_target(pet);
        }

        if (!pet_loot_target_oid && pet_loot_cooldown == 0)
        {
            MapDrops::PetLootQuery query = {
                player.get_position(),
                player.get_oid(),
                current_party_id(),
                120,
                capabilities.meso_magnet,
                capabilities.item_pouch,
                capabilities.magic_scales
            };
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
        bool pet_in_range = pet_follow::distance_between(
            pet->get_position(), pet_loot_target_position) <= 50;
        if (pet_loot::should_attempt(pet_loot_target_frames,
            pet_loot_attempts, pet_in_range))
        {
            PetLootPacket(pet->get_uniqueid(), pet_loot_target_oid).dispatch();
            ++pet_loot_attempts;
            if (pet_loot::attempts_exhausted(pet_loot_attempts))
            {
                clear_pet_loot_target(pet);
                pet_loot_cooldown = PET_LOOT_COOLDOWN_FRAMES;
            }
        }
    }

    void Stage::update_pet_movement()
    {
        PetLook* pet = player.get_pet(0);
        if (!pet)
        {
            last_pet_movement = Movement();
            pet_movement_sync_frames = 0;
            return;
        }

        bool urgent = pet->consume_urgent_movement();
        if (!urgent && pet_movement_sync_frames > 0)
        {
            --pet_movement_sync_frames;
            return;
        }

        Movement current = pet->get_movement();
        if (urgent || last_pet_movement.type == Movement::NONE ||
            last_pet_movement.hasmoved(current))
        {
            MovePetPacket(pet->get_uniqueid(), current).dispatch();
            last_pet_movement = current;
            pet_movement_sync_frames = PET_MOVEMENT_SYNC_FRAMES;
        }
    }

    Cursor::State Stage::send_cursor(bool pressed, Point<int16_t> position)
    {
        if (state != ACTIVE)
        {
            return Cursor::IDLE;
        }
        return npcs.send_cursor(pressed, position, camera.position());
    }

    std::optional<PlayerTarget> Stage::find_player_target(Point<int16_t> position) const
    {
        if (state != ACTIVE)
        {
            return std::nullopt;
        }
        const Point<int16_t> view_position = camera.position();
        if (npcs.contains_cursor(position, view_position))
        {
            return std::nullopt;
        }
        return chars.find_target(position, view_position);
    }

    std::optional<PlayerTarget> Stage::find_character_info_target(
        Point<int16_t> position) const
    {
        if (state != ACTIVE)
            return std::nullopt;
        const Point<int16_t> view_position = camera.position();
        if (npcs.contains_cursor(position, view_position))
            return std::nullopt;
        const PlayerTarget self_target = {
            player.get_oid(), player.get_name(), player.get_level(),
            player.get_position()
        };
        return chars.find_target_with(position, view_position, self_target);
    }

    bool Stage::is_player(int32_t cid) const
    {
        return cid == player.get_oid();
    }

    Point<int16_t> Stage::get_view_position() const
    {
        return camera.position();
    }

    MapNpcs& Stage::get_npcs()
    {
        return npcs;
    }

    MapChars& Stage::get_chars()
    {
        return chars;
    }

    MapMobs& Stage::get_mobs()
    {
        return mobs;
    }

    MapReactors& Stage::get_reactors()
    {
        return reactors;
    }

    MapDrops& Stage::get_drops()
    {
        return drops;
    }

    MapMists& Stage::get_mists()
    {
        return mists;
    }

    Player& Stage::get_player()
    {
        return player;
    }

    int32_t Stage::get_mapid() const
    {
        return mapid;
    }

    Combat& Stage::get_combat()
    {
        return combat;
    }

    void Stage::set_pet_auto_hp_item(int32_t item_id)
    {
        pet_automation.set_hp_item(item_id);
    }

    void Stage::set_pet_auto_mp_item(int32_t item_id)
    {
        pet_automation.set_mp_item(item_id);
    }

    bool Stage::set_pet_auto_thresholds(uint8_t hp, uint8_t mp)
    {
        pet_settings::Thresholds thresholds = {
            jrc::pet_automation::clamp_threshold(hp),
            jrc::pet_automation::clamp_threshold(mp)
        };
        if (!pet_settings::save(thresholds))
            return false;

        pet_automation.set_hp_threshold(thresholds.hp);
        pet_automation.set_mp_threshold(thresholds.mp);
        return true;
    }

    const pet_automation::Controller& Stage::get_pet_automation() const
    {
        return pet_automation;
    }

    pet_capability::Capabilities Stage::get_pet_capabilities() const
    {
        const PetLook* pet = player.get_pet(0);
        const Inventory& inventory = player.get_inventory();
        return pet_capability::resolve(pet ? pet->get_itemid() : 0, {
            inventory.has_equipped_item(pet_automation::MESO_MAGNET),
            inventory.has_equipped_item(pet_automation::ITEM_POUCH),
            inventory.has_equipped_item(pet_automation::AUTO_HP_POUCH),
            inventory.has_equipped_item(pet_automation::AUTO_MP_POUCH),
            inventory.has_equipped_item(pet_automation::WING_BOOTS),
            inventory.has_equipped_item(pet_automation::MAGIC_SCALES)
        });
    }

    int32_t Stage::get_pet_loot_target_oid() const
    {
        return pet_loot_target_oid;
    }

    Optional<Char> Stage::get_character(int32_t cid)
    {
        if (is_player(cid))
        {
            return player;
        }
        else
        {
            return chars.get_char(cid);
        }
    }

    void Stage::add_effect(const std::string& path)
    {
        effect = MapEffect(path);
        if (is_intro_input_locked())
        {
            player.set_direction(false);
            release_actions();
        }
    }

    void Stage::schedule_intro_warp(int32_t target_mapid, int32_t delay_ms)
    {
        pending_intro_warp_mapid = target_mapid;
        pending_intro_warp_delay_ms = std::max<int32_t>(0, delay_ms);
    }

    void Stage::update_intro_warp()
    {
        if (pending_intro_warp_mapid < 0)
        {
            return;
        }

        pending_intro_warp_delay_ms -= Constants::TIMESTEP;
        if (pending_intro_warp_delay_ms > 0)
        {
            return;
        }

        ChangeMapPacket(false, pending_intro_warp_mapid, "", false).dispatch();
        pending_intro_warp_mapid = -1;
        pending_intro_warp_delay_ms = 0;
    }
}
