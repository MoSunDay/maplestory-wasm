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
#pragma once
#include "Attack.h"
#include "FinalAttack.h"
#include "RegularAttack.h"
#include "Skill.h"

#include "../MapleMap/MapChars.h"
#include "../MapleMap/MapDrops.h"
#include "../MapleMap/MapMobs.h"
#include "../Physics/Physics.h"

#include "../../Character/Player.h"
#include "../../Template/TimedQueue.h"

#include <cstdint>

namespace jrc
{
    class Combat
    {
    public:
        Combat(Player& player, MapChars& chars, MapMobs& mobs,
            MapDrops& drops, Physics& physics);

        // Draw bullets, damage numbers etc.
        void draw(double viewx, double viewy, float alpha) const;
        // Poll attacks, damage effects, etc.
        void update();

        // Make the player use a special move. Returns true if the move was used.
        bool use_move(int32_t move_id);
        // Queue a final-attack follow-up roll after a successful skill attack.
        void schedule_final_attack(const SpecialMove& move,
            const Attack& attack, const AttackResult& result);
        // Reset transient state (e.g. on map change).
        void clear();

        // Add an attack to the attack queue.
        void push_attack(const AttackResult& attack);
        // Show a buff effect.
        void show_buff(int32_t cid, int32_t skillid, int8_t level);
        // Show a buff effect.
        void show_player_buff(int32_t skillid);
        // Show damage a remote source dealt to a mob (e.g. damage over time).
        void show_damage(int32_t oid, int32_t damage);

        // Read-only probe access for tests: steady-clock ms of the last
        // successful cast of a throttled buff skill, 0 if never cast.
        uint64_t probe_last_buffcast_ms(int32_t skillid) const
        {
            auto iter = last_buffcast_ms.find(skillid);
            return iter == last_buffcast_ms.end() ? 0 : iter->second;
        }

    private:
        struct DamageEffect
        {
            AttackUser user;
            DamageNumber number;
            int32_t damage;
            bool toleft;
            int32_t target_oid;
            int32_t move_id;
        };

        struct BulletEffect
        {
            DamageEffect damageeffect;
            Bullet bullet;
            Point<int16_t> target;
        };

        void apply_attack(const AttackResult& attack);
        void apply_move(const SpecialMove& move);
        void apply_use_movement(const SpecialMove& move);
        void apply_result_movement(const SpecialMove& move, const Attack& attack);
        void apply_rush(const Attack& attack);
        void apply_bullet_effect(const BulletEffect& effect);
        void apply_damage_effect(const DamageEffect& effect);
        void extract_effects(const Char& user, const SpecialMove& move, const AttackResult& result);
        std::vector<DamageNumber> place_numbers(int32_t oid, const std::vector<std::pair<int32_t, bool>>& damagelines);
        const SpecialMove& get_move(int32_t move_id);

        static bool is_teleport_skill(int32_t skillid);
        static bool is_flash_jump(int32_t skillid);
        // Dash forward with Flash Jump; one use per airborne period.
        void apply_flash_jump();
        bool apply_teleport(const SpecialMove& move);
        Point<int16_t> find_teleport_target(int16_t range);
        Point<int16_t> find_teleport_target_vertical(bool up, int16_t range);
        Point<int16_t> find_teleport_target_horizontal(int16_t range);

        // Fire a proc'd final attack after the primary swing ends.
        void perform_final_attack();

        Player& player;
        MapChars& chars;
        MapMobs& mobs;
        MapDrops& drops;
        Physics& physics;

        std::unordered_map<int32_t, Skill> skills;
        RegularAttack regularattack;
        int16_t teleport_cooldown = 0;
        bool flashjump_spent = false;
        // Steady-clock ms of the last successful cast per non-attack skill.
        // Buff skills without a cast action (e.g. Iron Body) never enter the
        // attacking state, so held or auto-repeated keydowns would re-cast on
        // every event without this gate.
        std::unordered_map<int32_t, uint64_t> last_buffcast_ms;

        // Skill id of a proc'd final attack waiting to fire, or 0.
        int32_t final_attack_pending = 0;
        std::vector<int32_t> final_attack_targets;

        TimedQueue<AttackResult> attackresults;
        TimedQueue<BulletEffect> bulleteffects;
        TimedQueue<DamageEffect> damageeffects;

        std::list<BulletEffect> bullets;
        std::list<DamageNumber> damagenumbers;
    };
}
