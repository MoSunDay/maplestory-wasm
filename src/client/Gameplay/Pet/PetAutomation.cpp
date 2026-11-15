#include "PetAutomation.h"

#include <algorithm>

namespace jrc::pet_automation
{
    uint8_t clamp_threshold(int32_t value)
    {
        return static_cast<uint8_t>(std::clamp(value, 1, 99));
    }

    bool at_or_below_threshold(int32_t current, int32_t maximum, uint8_t threshold)
    {
        if (maximum <= 0 || current < 0)
            return false;
        return static_cast<int64_t>(current) * 100 <=
            static_cast<int64_t>(maximum) * clamp_threshold(threshold);
    }

    int32_t select_next_item(const std::vector<int32_t>& candidates,
        int32_t current_item, int32_t direction)
    {
        if (candidates.empty())
            return 0;

        auto current = std::find(candidates.begin(), candidates.end(), current_item);
        if (current == candidates.end())
            return candidates.front();

        const int64_t size = static_cast<int64_t>(candidates.size());
        const int64_t index = std::distance(candidates.begin(), current);
        const int64_t step = direction < 0 ? -1 : 1;
        return candidates[static_cast<size_t>((index + step + size) % size)];
    }

    BuffSkillSlots sanitize_buff_skills(const BuffSkillSlots& skills)
    {
        BuffSkillSlots sanitized{};
        for (size_t slot = 0; slot < skills.size(); ++slot)
        {
            const int32_t skill_id = skills[slot];
            if (skill_id <= 0)
                continue;
            if (std::find(sanitized.begin(), sanitized.end(), skill_id) !=
                sanitized.end())
                continue;
            sanitized[slot] = skill_id;
        }
        return sanitized;
    }

    Controller::Controller(uint8_t hp_value, uint8_t mp_value)
        : hp_threshold(clamp_threshold(hp_value)),
          mp_threshold(clamp_threshold(mp_value))
    {
    }

    void Controller::tick()
    {
        if (hp_cooldown > 0)
            --hp_cooldown;
        if (mp_cooldown > 0)
            --mp_cooldown;
        if (food_cooldown > 0)
            --food_cooldown;
        for (uint16_t& cooldown : buff_cooldowns)
        {
            if (cooldown > 0)
                --cooldown;
        }
        for (uint16_t& backoff : buff_backoff)
        {
            if (backoff > 0)
                --backoff;
        }

        if (!pending_buff)
            return;

        if (pending_frames > 0)
            --pending_frames;
        if (pending_frames > 0)
            return;

        const size_t slot = pending_slot;
        clear_pending_buff();
        if (slot >= buff_skills.size() || buff_skills[slot] <= 0)
            return;

        if (buff_failures[slot] < BUFF_MAX_FAILURES)
            ++buff_failures[slot];
        buff_backoff[slot] = BUFF_FAILURE_BACKOFF_FRAMES;
        if (buff_failures[slot] >= BUFF_MAX_FAILURES)
            buff_unconfirmed[slot] = true;
    }

    void Controller::set_hp_item(int32_t item_id) { hp_item = std::max(item_id, 0); }
    void Controller::set_mp_item(int32_t item_id) { mp_item = std::max(item_id, 0); }
    void Controller::set_food_item(int32_t item_id) { food_item = std::max(item_id, 0); }
    void Controller::set_hp_threshold(uint8_t value) { hp_threshold = clamp_threshold(value); }
    void Controller::set_mp_threshold(uint8_t value) { mp_threshold = clamp_threshold(value); }
    int32_t Controller::get_hp_item() const { return hp_item; }
    int32_t Controller::get_mp_item() const { return mp_item; }
    int32_t Controller::get_food_item() const { return food_item; }
    uint8_t Controller::get_hp_threshold() const { return hp_threshold; }
    uint8_t Controller::get_mp_threshold() const { return mp_threshold; }

    void Controller::set_buff_skill(size_t slot, int32_t skill_id)
    {
        if (slot >= buff_skills.size())
            return;

        int32_t id = std::max(skill_id, 0);
        if (id > 0)
        {
            // A skill can only occupy one slot; moving it clears the old one.
            for (size_t i = 0; i < buff_skills.size(); ++i)
            {
                if (i != slot && buff_skills[i] == id)
                {
                    buff_skills[i] = 0;
                    buff_cooldowns[i] = 0;
                    buff_backoff[i] = 0;
                    buff_failures[i] = 0;
                    buff_unconfirmed[i] = false;
                    if (pending_buff && pending_slot == i)
                        clear_pending_buff();
                }
            }
        }

        if (buff_skills[slot] != id)
        {
            buff_cooldowns[slot] = 0;
            buff_backoff[slot] = 0;
            buff_failures[slot] = 0;
            buff_unconfirmed[slot] = false;
            if (pending_buff && pending_slot == slot)
                clear_pending_buff();
        }
        buff_skills[slot] = id;
    }

    void Controller::set_buff_skills(const BuffSkillSlots& skills)
    {
        buff_skills = {};
        buff_cooldowns = {};
        clear_buff_transient();
        for (size_t i = 0; i < skills.size() && i < buff_skills.size(); ++i)
            set_buff_skill(i, skills[i]);
    }

    int32_t Controller::get_buff_skill(size_t slot) const
    {
        return slot < buff_skills.size() ? buff_skills[slot] : 0;
    }

    const BuffSkillSlots& Controller::get_buff_skills() const
    {
        return buff_skills;
    }

    size_t Controller::get_buff_skill_count() const
    {
        return static_cast<size_t>(std::count_if(
            buff_skills.begin(), buff_skills.end(),
            [](int32_t skill_id) { return skill_id > 0; }));
    }

    AutoPotDecisions Controller::decide(const AutoPotInputs& inputs)
    {
        AutoPotDecisions result;
        if (!inputs.alive || !inputs.pet_active)
            return result;

        bool use_hp = hp_item > 0 && hp_cooldown == 0 && inputs.hp_ability &&
            inputs.hp_item_available &&
            at_or_below_threshold(inputs.hp, inputs.max_hp, hp_threshold);
        bool use_mp = mp_item > 0 && mp_cooldown == 0 && inputs.mp_ability &&
            inputs.mp_item_available &&
            at_or_below_threshold(inputs.mp, inputs.max_mp, mp_threshold);

        if (use_hp && use_mp && hp_item == mp_item)
        {
            result.requests[0] = { Resource::BOTH, hp_item };
            result.count = 1;
            hp_cooldown = POTION_COOLDOWN_FRAMES;
            mp_cooldown = POTION_COOLDOWN_FRAMES;
            return result;
        }
        if (use_hp)
        {
            result.requests[result.count++] = { Resource::HP, hp_item };
            hp_cooldown = POTION_COOLDOWN_FRAMES;
            return result;
        }
        if (use_mp)
        {
            result.requests[result.count++] = { Resource::MP, mp_item };
            mp_cooldown = POTION_COOLDOWN_FRAMES;
        }
        return result;
    }

    int32_t Controller::decide_feed(const AutoFeedInputs& inputs)
    {
        if (food_item <= 0 || food_cooldown > 0 || !inputs.pet_active ||
            !inputs.hungry || !inputs.food_available)
            return 0;

        food_cooldown = FEED_COOLDOWN_FRAMES;
        return food_item;
    }

    bool Controller::decide_buff(size_t slot, const AutoBuffInputs& inputs)
    {
        if (slot >= buff_skills.size() || buff_skills[slot] <= 0 ||
            buff_cooldowns[slot] > 0 || buff_backoff[slot] > 0 ||
            buff_unconfirmed[slot] || !inputs.pet_active || !inputs.alive ||
            inputs.buff_active || !inputs.skill_ready || !inputs.mp_ok ||
            !inputs.castable)
            return false;

        buff_cooldowns[slot] = BUFF_CAST_COOLDOWN_FRAMES;
        pending_buff = true;
        pending_slot = slot;
        pending_skill = buff_skills[slot];
        pending_frames = BUFF_CONFIRM_TIMEOUT_FRAMES;
        return true;
    }

    void Controller::confirm_buff(int32_t skill_id)
    {
        if (skill_id <= 0)
            return;

        if (pending_buff && pending_skill == skill_id)
            clear_pending_buff();

        for (size_t slot = 0; slot < buff_skills.size(); ++slot)
        {
            if (buff_skills[slot] != skill_id)
                continue;
            buff_backoff[slot] = 0;
            buff_failures[slot] = 0;
        }
    }

    void Controller::reset_transient()
    {
        hp_cooldown = 0;
        mp_cooldown = 0;
        food_cooldown = 0;
        buff_cooldowns.fill(0);
        clear_buff_transient();
    }

    void Controller::clear_pending_buff()
    {
        pending_buff = false;
        pending_slot = 0;
        pending_skill = 0;
        pending_frames = 0;
    }

    void Controller::clear_buff_transient()
    {
        buff_backoff.fill(0);
        buff_failures.fill(0);
        buff_unconfirmed.fill(false);
        clear_pending_buff();
    }

    bool Controller::has_pending_buff() const
    {
        return pending_buff;
    }

    size_t Controller::get_pending_slot() const
    {
        return pending_slot;
    }

    int32_t Controller::get_pending_skill() const
    {
        return pending_skill;
    }

    uint16_t Controller::get_pending_frames() const
    {
        return pending_frames;
    }

    uint16_t Controller::get_buff_backoff(size_t slot) const
    {
        return slot < buff_backoff.size() ? buff_backoff[slot] : 0;
    }

    uint8_t Controller::get_buff_failures(size_t slot) const
    {
        return slot < buff_failures.size() ? buff_failures[slot] : 0;
    }

    bool Controller::is_buff_unconfirmed(size_t slot) const
    {
        return slot < buff_unconfirmed.size() && buff_unconfirmed[slot];
    }
}
