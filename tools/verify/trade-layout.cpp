#include "IO/Trade/TradeLayout.h"

#include <cassert>
#include <iostream>

int main()
{
    using namespace jrc::trade_layout;

    static_assert(WIDTH == 654, "trade window width must match the NX background");
    static_assert(HEIGHT == 566, "trade window height must match the NX background");
    static_assert(LOCAL_NAME_X == 85 && PARTNER_NAME_X == 315,
        "name tags must be centred on the two NX plates");
    static_assert(NAME_Y > LOOK_Y && NAME_Y < ITEM_TOP,
        "name tags must sit between the character previews and the item grid");
    static_assert(LOCAL_ITEM_X == 49 && PARTNER_ITEM_X == 242,
        "item columns must start inside the two NX slot grids");
    static_assert(ITEM_TOP == 151 && ITEM_STEP == 40,
        "item rows must follow the 40px NX slot pitch");
    static_assert(LOCAL_ITEM_X + ITEM_LABEL_DX + ITEM_LABEL_WIDTH <=
        PARTNER_ITEM_X, "labels must not enter the partner column");
    static_assert(PARTNER_ITEM_X + ITEM_LABEL_DX + ITEM_LABEL_WIDTH <= 389,
        "partner labels must stay inside the right slot grid");
    static_assert(ITEM_TOP + 4 * ITEM_STEP + 32 <= 345,
        "five item rows must fit above the meso row");
    static_assert(MESO_Y == 364,
        "meso labels must start on the NX white meso row");
    static_assert(PAGE_Y + 14 <= ITEM_TOP,
        "page label must fit in the gap above the item grid");
    static_assert(CHAT_X + CHAT_WIDTH <= 640,
        "chat text must stay inside the right NX panel");
    static_assert(CHAT_Y + 6 * CHAT_STEP <= CHATFIELD.t(),
        "six chat lines must fit above the input field");

    assert(CHATFIELD.l() == 409 && CHATFIELD.r() == 578);
    assert(CHATFIELD.t() == 364 && CHATFIELD.b() == 378);
    assert(LOCAL_MESO_X > 29 && LOCAL_MESO_X < 195);
    assert(PARTNER_MESO_X > 222 && PARTNER_MESO_X < 388);

    std::cout << "PASS trade window layout follows CashTradingRoom NX geometry\n";
    return 0;
}
