#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    struct TradeOffer
    {
        int32_t item_id = 0;
        int16_t quantity = 0;
    };

    class TradeState
    {
    public:
        static constexpr uint8_t OFFER_SLOTS = 9;

        void set_name(bool partner, const std::string& name);
        void set_item(bool partner, uint8_t slot, TradeOffer offer);
        void set_meso(bool partner, int32_t amount);
        void add_chat(const std::string& message);
        void confirm(bool partner);

        const std::string& name(bool partner) const;
        const TradeOffer& item(bool partner, uint8_t slot) const;
        int32_t meso(bool partner) const;
        bool confirmed(bool partner) const;
        uint8_t next_local_slot() const;
        const std::vector<std::string>& chat() const;

    private:
        std::array<std::string, 2> names;
        std::array<std::array<TradeOffer, OFFER_SLOTS>, 2> offers = {};
        std::array<int32_t, 2> mesos = {};
        std::array<bool, 2> confirmations = {};
        std::vector<std::string> messages;
    };
}
