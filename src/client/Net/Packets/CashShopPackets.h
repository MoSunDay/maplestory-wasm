#pragma once

#include "../OutPacket.h"
#include "../../Character/Inventory/InventoryType.h"
#include "../../IO/CashShop/CashShopModel.h"

namespace jrc
{
    class EnterCashShopPacket : public OutPacket
    {
    public:
        EnterCashShopPacket() : OutPacket(ENTER_CASHSHOP) {}
    };

    /// Requests the player's current Cash Shop balances.
    /// Opcode: CHECK_CASH(0xE4); server replies with QUERY_CASH_RESULT.
    class CheckCashPacket : public OutPacket
    {
    public:
        CheckCashPacket() : OutPacket(CHECK_CASH) {}
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

    /// Buys four extra slots for one inventory tab.
    /// Opcode: CASHSHOP_OPERATION(0x06); the server answers a successful
    /// purchase with 0x60 (tab + new slot maximum) and then pushes the
    /// balances (0x144). A rejection is the balance push alone.
    class BuyCashInventorySlotsPacket : public OutPacket
    {
    public:
        BuyCashInventorySlotsPacket(CashCurrency currency,
            InventoryType::Id inventory_type)
            : OutPacket(CASHSHOP_OPERATION)
        {
            write_byte(0x06);
            write_byte(0);
            write_int(static_cast<int32_t>(currency));
            write_byte(0);
            write_byte(static_cast<int8_t>(inventory_type));
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
