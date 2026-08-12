#pragma once

#include "../OutPacket.h"
#include "../../IO/CashShop/CashShopModel.h"

namespace jrc
{
    class EnterCashShopPacket : public OutPacket
    {
    public:
        EnterCashShopPacket() : OutPacket(ENTER_CASHSHOP) {}
    };

    class LeaveCashShopPacket : public OutPacket
    {
    public:
        LeaveCashShopPacket() : OutPacket(CHANGEMAP) {}
    };

    class BuyCashItemPacket : public OutPacket
    {
    public:
        BuyCashItemPacket(int32_t sn, CashCurrency currency, bool package)
            : OutPacket(CASHSHOP_OPERATION)
        {
            write_byte(package ? 0x1E : 0x03);
            write_byte(0);
            write_int(static_cast<int32_t>(currency));
            write_int(sn);
        }
    };

    class TakeCashLockerItemPacket : public OutPacket
    {
    public:
        explicit TakeCashLockerItemPacket(int64_t cash_id)
            : OutPacket(CASHSHOP_OPERATION)
        {
            write_byte(0x0D);
            write_int(static_cast<int32_t>(cash_id));
        }
    };

    class PutCashLockerItemPacket : public OutPacket
    {
    public:
        PutCashLockerItemPacket(int64_t cash_id, int8_t inventory_type)
            : OutPacket(CASHSHOP_OPERATION)
        {
            write_byte(0x0E);
            write_long(cash_id);
            write_byte(inventory_type);
        }
    };
}
