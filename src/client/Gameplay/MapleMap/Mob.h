#pragma once
#include "LivingObject.h"

#include "../Combat/Attack.h"
#include "../Combat/Bullet.h"
#include "../Combat/DamageNumber.h"
#include "../MobCombat/MobAction.h"
#include "../Movement.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/EffectLayer.h"
#include "../../Graphics/Geometry.h"
#include "../../Template/Rectangle.h"
#include "../../Template/Interpolated.h"
#include "../../Util/Randomizer.h"
#include "../../Util/TimedBool.h"

#include <unordered_map>


namespace jrc
{
    class Mob : public LivingObject
    {
    public:
        static const size_t NUM_STANCES = 6;
        enum Stance : uint8_t
        {
            MOVE  = 2,
            STAND = 4,
            JUMP  = 6,
            HIT   = 8,
            DIE   = 10
        };

        static std::string nameof(Stance stance)
        {
            static const std::string stancenames[NUM_STANCES] =
            {
                "move", "stand", "jump", "hit1", "die1", "fly"
            };
            size_t index = (stance - 1) / 2;
            return stancenames[index];
        }

        static uint8_t value_of(Stance stance, bool flip)
        {
            return flip ? stance : stance + 1;
        }

        /// Construct a mob by combining data from game files with
        /// data sent by the server.
        Mob(int32_t        oid,
            int32_t        mobid,
            int8_t         mode,
            uint8_t        stance,
            uint16_t       fhid,
            bool           newspawn,
            int8_t         team,
            Point<int16_t> position);

        /// Draw the mob.
        void draw(double viewx, double viewy, float alpha) const override;
        /// Update movement and animations.
        int8_t update(const Physics& physics) override;
        /// Update the local aggro target used by a controlled mob.
        void set_aggro_target(Point<int16_t> target);
        /// Store the one-use skill suggestion returned by the server.
        void suggest_skill(int16_t moveid, bool aggro,
                           uint8_t skill_id, uint8_t skill_level);

        /// Change this mob's control mode:
        /// 0 - no control, 1 - control, 2 - aggro
        void set_control(int8_t mode);
        /// Send movement to the mob.
        void send_movement(Point<int16_t> start, uint8_t raw_activity,
                           uint8_t skill_id, uint8_t skill_level,
                           std::vector<Movement>&& movements);
        /// Kill the mob with the appropriate type:
        /// 0 - make inactive 1 - death animation 2 - fade out
        void kill(int8_t killtype);
        /// Display the hp percentage above the mob.
        /// Use the playerlevel to determine color of nametag.
        void show_hp(int8_t percentage, uint16_t playerlevel);
        /// Show an effect at the mob's position.
        void show_effect(const Animation& animation, int8_t pos, int8_t z, bool flip);
        void apply_status(uint32_t first_mask, uint32_t second_mask,
                          const std::vector<int16_t>& values);
        void cancel_status(uint32_t first_mask, uint32_t second_mask);
        void show_status_skill(uint16_t skill_id, uint16_t skill_level);

        /// Calculate the damage to this mob with the spcecified attack.
        std::vector<std::pair<int32_t, bool>> calculate_damage(const Attack& attack);
        /// Apply damage to the mob.
        void apply_damage(int32_t damage, bool toleft);

        /// Create a touch damage attack to the player.
        MobAttack create_touch_attack() const;
        /// Return a normal-attack hit once its WZ effect frame is reached.
        MobAttack poll_action_attack(Point<int16_t> target);

        /// Check if this mob collides with the specified rectangle.
        bool is_in_range(const Rectangle<int16_t>& range) const;
        /// Check if this mob is still alive.
        bool is_alive() const;
        /// Return the body center used for combat targeting.
        Point<int16_t> get_body_position() const;
        /// Return the head position.
        Point<int16_t> get_head_position() const;

    private:
        enum FlyDirection
        {
            STRAIGHT,
            UPWARDS,
            DOWNWARDS,
            NUM_DIRECTIONS
        };

        struct NormalAttack
        {
            uint8_t index = 0;
            bool magic = false;
            uint16_t effect_after = 0;
            uint16_t cooldown = 0;
            mob_action::Range range;
            Animation animation;
        };

        struct SkillAction
        {
            uint8_t id = 0;
            uint8_t level = 0;
            uint8_t action = 0;
            uint16_t effect_after = 0;
            mob_action::Range range;
            Animation animation;
        };

        enum class ActionKind : uint8_t
        {
            NONE,
            ATTACK,
            SKILL
        };

        /// Set the stance by byte value.
        void set_stance(uint8_t stancebyte);
        /// Set the stance by enum value.
        void set_stance(Stance newstance);
        Animation& current_animation();
        const Animation& current_animation() const;
        void load_combat(const nl::node& source, const nl::node& visual_source);
        void update_aggro_action();
        void begin_attack(size_t attack_pos, bool notify_server);
        void begin_skill(size_t skill_pos, bool notify_server);
        void finish_action();
        void cancel_action();
        bool action_in_range(const mob_action::Range& range) const;
        int16_t status_value(uint32_t mask) const;
        /// Start the death animation.
        void apply_death();
        /// Decide on the next state.
        void next_move();
        /// Send the current position and state to the server.
        void update_movement();

        /// Calculate the hit chance.
        float calculate_hitchance(int16_t leveldelta, int32_t accuracy) const;
        /// Calculate the minimum damage.
        double calculate_mindamage(int16_t leveldelta, double mindamage, bool magic) const;
        /// Calculate the maximum damage.
        double calculate_maxdamage(int16_t leveldelta, double maxdamage, bool magic) const;
        /// Calculate a random damage line based on the specified values.
        std::pair<int32_t, bool> next_damage(double mindamage,
                                             double maxdamage,
                                             float  hitchance,
                                             float  critical) const;

        /// Return the current 'head' position.
        Point<int16_t> get_head_position(Point<int16_t> position) const;
        /// Return the current combat-targeting position.
        Point<int16_t> get_body_position(Point<int16_t> position) const;
        /// Return the current animation bounds in world space.
        Rectangle<int16_t> get_bounds(Point<int16_t> position) const;

        std::map<Stance, Animation> animations;
        std::string name;
        Sound hitsound;
        Sound diesound;
        uint16_t level = 0;
        float speed = 0.0f;
        float flyspeed = 0.0f;
        uint16_t watk = 0;
        uint16_t matk = 0;
        uint16_t wdef = 0;
        uint16_t mdef = 0;
        uint16_t accuracy = 0;
        uint16_t avoid = 0;
        uint16_t knockback = 0;
        bool undead = false;
        bool touchdamage = false;
        bool noflip = false;
        bool notattack = false;
        bool canmove = false;
        bool canjump = false;
        bool canfly = false;

        EffectLayer effects;
        Text namelabel;
        MobHpBar hpbar;
        Randomizer randomizer;

        TimedBool showhp;

        std::vector<Movement> movements;
        uint16_t counter = 0;
        uint16_t moveid = 0;

        std::vector<NormalAttack> normal_attacks;
        std::vector<SkillAction> skill_actions;
        std::vector<uint16_t> attack_cooldowns;
        Point<int16_t> aggro_target;
        uint8_t suggested_skill_id = 0;
        uint8_t suggested_skill_level = 0;
        size_t next_attack = 0;
        size_t active_action_pos = 0;
        uint16_t action_elapsed = 0;
        uint16_t action_hit_delay = 0;
        bool action_hit_pending = false;
        ActionKind action_kind = ActionKind::NONE;
        uint32_t status_first_mask = 0;
        uint32_t status_second_mask = 0;
        std::unordered_map<uint32_t, int16_t> status_values;

        int32_t id = 0;
        int8_t effect = 0;
        int8_t team = 0;
        bool dying = false;
        bool dead = false;
        bool awaitdeath = false;
        bool control = false;
        bool aggro = false;
        Stance stance = STAND;
        bool flip = false;
        FlyDirection flydirection = STRAIGHT;
        float walkforce = 0.0f;
        int8_t hppercent = 0;
        bool fading = false;
        bool fadein = false;
        Linear<float> opacity;
    };
}
