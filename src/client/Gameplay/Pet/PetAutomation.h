#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace jrc::pet_automation
{
    constexpr int32_t MESO_MAGNET = 1812000;
    constexpr int32_t ITEM_POUCH = 1812001;
    constexpr int32_t AUTO_HP_POUCH = 1812002;
    constexpr int32_t AUTO_MP_POUCH = 1812003;
    constexpr int32_t WING_BOOTS = 1812004;
    constexpr int32_t BINOCULAR = 1812005;
    constexpr int32_t MAGIC_SCALES = 1812006;

    enum class Resource : uint8_t
    {
        HP,
        MP,
        BOTH
    };

    struct AutoPotInputs
    {
        int32_t hp = 0;
        int32_t max_hp = 0;
        int32_t mp = 0;
        int32_t max_mp = 0;
        bool alive = false;
        bool pet_active = false;
        bool hp_ability = false;
        bool mp_ability = false;
        bool hp_item_available = false;
        bool mp_item_available = false;
    };

    struct AutoPotRequest
    {
        Resource resource = Resource::HP;
        int32_t item_id = 0;
    };

    struct AutoPotDecisions
    {
        std::array<AutoPotRequest, 2> requests{};
        size_t count = 0;
    };

    struct AutoFeedInputs
    {
        bool pet_active = false;
        bool hungry = false;
        bool food_available = false;
    };

    constexpr size_t MAX_BUFF_SKILLS = 12;
    using BuffSkillSlots = std::array<int32_t, MAX_BUFF_SKILLS>;

    struct AutoBuffInputs
    {
        bool pet_active = false;
        bool alive = false;
        bool buff_active = false;
        bool skill_ready = false;
        // Class/policy gates and MP affordability. Defaults keep legacy
        // aggregate callers working; the stage always supplies both.
        bool mp_ok = true;
        bool castable = true;
    };

    // Pet automation phase order. Buffs run before the hungry gate so a
    // hungry pet still keeps its buffs up while looting/potions pause.
    enum class UpdatePhase : uint8_t
    {
        TICK,
        ENSURE_PET,
        FEED,
        BUFFS,
        HUNGRY_EXIT,
        LOOT,
        POTION
    };

    constexpr bool buffs_run_before_hungry_exit()
    {
        return static_cast<uint8_t>(UpdatePhase::BUFFS) <
            static_cast<uint8_t>(UpdatePhase::HUNGRY_EXIT);
    }

    // Keep the first occurrence of each positive skill id.
    BuffSkillSlots sanitize_buff_skills(const BuffSkillSlots& skills);

    uint8_t clamp_threshold(int32_t value);
    bool at_or_below_threshold(int32_t current, int32_t maximum, uint8_t threshold);
    int32_t select_next_item(const std::vector<int32_t>& candidates,
        int32_t current_item, int32_t direction);

    class Controller
    {
    public:
        Controller(uint8_t hp_threshold = 50, uint8_t mp_threshold = 50);

        void tick();
        void set_hp_item(int32_t item_id);
        void set_mp_item(int32_t item_id);
        void set_food_item(int32_t item_id);
        void set_hp_threshold(uint8_t threshold);
        void set_mp_threshold(uint8_t threshold);
        int32_t get_hp_item() const;
        int32_t get_mp_item() const;
        int32_t get_food_item() const;
        uint8_t get_hp_threshold() const;
        uint8_t get_mp_threshold() const;
        void set_buff_skill(size_t slot, int32_t skill_id);
        void set_buff_skills(const BuffSkillSlots& skills);
        int32_t get_buff_skill(size_t slot) const;
        const BuffSkillSlots& get_buff_skills() const;
        size_t get_buff_skill_count() const;
        AutoPotDecisions decide(const AutoPotInputs& inputs);
        // Returns the food item to feed now, or 0 when the pet is fed,
        // satisfied, has no food available, or is still on feed cooldown.
        int32_t decide_feed(const AutoFeedInputs& inputs);
        // Returns true when the slot's buff should be recast now. The local
        // debounce keeps a cast request from being sent every frame while the
        // server round-trip and GIVE_BUFF packet are in flight. A successful
        // decision records the pending cast.
        bool decide_buff(size_t slot, const AutoBuffInputs& inputs);
        // GIVE_BUFF confirmation: clears the matching pending cast, backoff
        // and failure count.
        void confirm_buff(int32_t skill_id);
        // Clears cooldowns, pending/failure/backoff state but preserves
        // thresholds, items, food and buff skill assignments.
        void reset_transient();

        bool has_pending_buff() const;
        size_t get_pending_slot() const;
        int32_t get_pending_skill() const;
        uint16_t get_pending_frames() const;
        uint16_t get_buff_backoff(size_t slot) const;
        uint8_t get_buff_failures(size_t slot) const;
        bool is_buff_unconfirmed(size_t slot) const;

        static constexpr uint16_t BUFF_CAST_COOLDOWN_FRAMES = 60;
        static constexpr uint16_t BUFF_CONFIRM_TIMEOUT_FRAMES = 90;
        static constexpr uint16_t BUFF_FAILURE_BACKOFF_FRAMES = 300;
        static constexpr uint8_t BUFF_MAX_FAILURES = 3;

    private:
        static constexpr uint16_t POTION_COOLDOWN_FRAMES = 45;
        static constexpr uint16_t FEED_COOLDOWN_FRAMES = 120;

        void clear_pending_buff();
        void clear_buff_transient();

        int32_t hp_item = 0;
        int32_t mp_item = 0;
        int32_t food_item = 0;
        BuffSkillSlots buff_skills{};
        uint8_t hp_threshold;
        uint8_t mp_threshold;
        uint16_t hp_cooldown = 0;
        uint16_t mp_cooldown = 0;
        uint16_t food_cooldown = 0;
        std::array<uint16_t, MAX_BUFF_SKILLS> buff_cooldowns{};
        std::array<uint16_t, MAX_BUFF_SKILLS> buff_backoff{};
        std::array<uint8_t, MAX_BUFF_SKILLS> buff_failures{};
        std::array<bool, MAX_BUFF_SKILLS> buff_unconfirmed{};
        bool pending_buff = false;
        size_t pending_slot = 0;
        int32_t pending_skill = 0;
        uint16_t pending_frames = 0;
    };
}
