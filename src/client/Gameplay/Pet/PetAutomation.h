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
        void set_hp_threshold(uint8_t threshold);
        void set_mp_threshold(uint8_t threshold);
        int32_t get_hp_item() const;
        int32_t get_mp_item() const;
        uint8_t get_hp_threshold() const;
        uint8_t get_mp_threshold() const;
        AutoPotDecisions decide(const AutoPotInputs& inputs);

    private:
        static constexpr uint16_t POTION_COOLDOWN_FRAMES = 45;

        int32_t hp_item = 0;
        int32_t mp_item = 0;
        uint8_t hp_threshold;
        uint8_t mp_threshold;
        uint16_t hp_cooldown = 0;
        uint16_t mp_cooldown = 0;
    };
}
