#include "TradeState.h"

namespace jrc
{
    void TradeState::set_name(bool partner, const std::string& value)
    {
        names[partner ? 1 : 0] = value;
    }

    void TradeState::set_item(bool partner, uint8_t slot, TradeOffer offer)
    {
        if (slot >= 1 && slot <= OFFER_SLOTS)
            offers[partner ? 1 : 0][slot - 1] = offer;
    }

    void TradeState::set_meso(bool partner, int32_t amount)
    {
        mesos[partner ? 1 : 0] = amount;
    }

    void TradeState::add_chat(const std::string& message)
    {
        messages.push_back(message);
        if (messages.size() > 6)
            messages.erase(messages.begin());
    }

    void TradeState::confirm(bool partner)
    {
        confirmations[partner ? 1 : 0] = true;
    }

    const std::string& TradeState::name(bool partner) const
    {
        return names[partner ? 1 : 0];
    }

    const TradeOffer& TradeState::item(bool partner, uint8_t slot) const
    {
        return offers[partner ? 1 : 0][slot - 1];
    }

    int32_t TradeState::meso(bool partner) const
    {
        return mesos[partner ? 1 : 0];
    }

    bool TradeState::confirmed(bool partner) const
    {
        return confirmations[partner ? 1 : 0];
    }

    uint8_t TradeState::next_local_slot() const
    {
        for (uint8_t index = 0; index < OFFER_SLOTS; ++index)
            if (offers[0][index].item_id == 0)
                return index + 1;
        return 0;
    }

    const std::vector<std::string>& TradeState::chat() const
    {
        return messages;
    }
}
