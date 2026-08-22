#include "IO/Trade/TradeState.h"

#include <cassert>

int main()
{
    jrc::TradeState trade;
    trade.set_name(false, "Local");
    trade.set_name(true, "Partner");
    assert(trade.name(false) == "Local");
    assert(trade.name(true) == "Partner");
    assert(trade.next_local_slot() == 1);

    for (uint8_t slot = 1; slot <= jrc::TradeState::OFFER_SLOTS; ++slot)
    {
        trade.set_item(false, slot, { 2000000 + slot, slot });
        assert(trade.item(false, slot).quantity == slot);
    }
    assert(trade.next_local_slot() == 0);

    trade.set_item(true, 9, { 4000000, 3 });
    assert(trade.item(true, 9).item_id == 4000000);
    trade.set_meso(false, 1000);
    trade.set_meso(true, 2000);
    assert(trade.meso(false) == 1000 && trade.meso(true) == 2000);

    for (int index = 0; index < 8; ++index)
        trade.add_chat(std::to_string(index));
    assert(trade.chat().size() == 6 && trade.chat().front() == "2");

    trade.confirm(false);
    trade.confirm(true);
    assert(trade.confirmed(false) && trade.confirmed(true));
}
