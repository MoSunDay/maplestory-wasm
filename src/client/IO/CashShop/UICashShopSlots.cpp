// Inventory slot-expansion flow of the cash shop: pill refresh, local
// validation, the confirmation dialog and the outgoing 0x06 request, plus
// settling the 0x60 success reply. Split from UICashShopActions.cpp to
// keep every cash-shop file inside the per-file line budget.
#include "UICashShop.h"

#include "../UI.h"
#include "../UITypes/UINotice.h"
#include "../../Character/Inventory/InventoryType.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"

namespace jrc
{
    namespace view = cash_shop_view;

    namespace
    {
        // Server-fixed pricing: every confirmed pill adds 4 slots to one
        // tab for 4000 of the currently selected currency, and each tab
        // caps at 128 slots.
        constexpr int32_t SLOT_PRICE = 4000;
        constexpr int16_t SLOT_STEP = 4;
        constexpr int16_t SLOT_MAX = 128;

        // Pill index 0..3 maps straight onto the EQUIP..ETC tab ids that
        // the CASHSHOP_OPERATION 0x06 request expects.
        InventoryType::Id slotbuy_type(size_t index)
        {
            return static_cast<InventoryType::Id>(InventoryType::EQUIP +
                static_cast<int8_t>(index % cash_shop_buttons::
                    SLOTBUY_CHOICES));
        }

        const char* slotbuy_name(size_t index)
        {
            static const char* const names[] =
                { "装备", "消耗", "设置", "其他" };
            return names[index % cash_shop_buttons::SLOTBUY_CHOICES];
        }

        // Confirmation copy for one tab, showing the exact slot change.
        std::string slot_question(CashCurrency currency,
            InventoryType::Id type, int16_t slotmax)
        {
            const size_t pill = static_cast<size_t>(
                type - InventoryType::EQUIP);
            return "使用 " + std::string(view::currency_name(currency)) +
                " 为" + slotbuy_name(pill) + "栏扩充 " +
                std::to_string(SLOT_STEP) + " 格（" +
                std::to_string(slotmax) + " → " +
                std::to_string(slotmax + SLOT_STEP) + "），价格 " +
                view::format_amount(SLOT_PRICE) + "？";
        }
    }

    void UICashShop::refresh_slotbuy()
    {
        const Inventory& inventory =
            Stage::get().get_player().get_inventory();
        for (size_t pill = 0; pill < cash_shop_buttons::SLOTBUY_CHOICES;
            ++pill)
        {
            const InventoryType::Id type = slotbuy_type(pill);
            const int16_t slotmax = inventory.get_slotmax(type);
            const bool purchasable = can_mutate() &&
                slotmax + SLOT_STEP <= SLOT_MAX;
            buttons[BT_SLOTBUY_FIRST + pill]->set_active(purchasable);
            slotbuy_labels[pill].change_text(
                std::string(slotbuy_name(pill)) + " " +
                std::to_string(slotmax) + "格");
            slotbuy_labels[pill].change_color(
                purchasable ? Text::BLACK : Text::LIGHTGREY);
        }
    }

    void UICashShop::request_slot_expansion(size_t pill_index)
    {
        // Same gate as request_purchase: loaded balances, no failure, no
        // pending operation and no required re-entry.
        if (!can_mutate() || pill_index >= cash_shop_buttons::SLOTBUY_CHOICES)
            return;
        const InventoryType::Id type = slotbuy_type(pill_index);
        const int16_t slotmax =
            Stage::get().get_player().get_inventory().get_slotmax(type);
        if (slotmax + SLOT_STEP > SLOT_MAX)
        {
            show_error("该栏格子已达上限。");
            return;
        }
        if (model->balances().get(currency) < SLOT_PRICE)
        {
            show_error("所选支付余额不足。");
            return;
        }
        const CashCurrency selected = currency;
        UI::get().emplace<UIYesNo>(
            slot_question(selected, type, slotmax),
            [this, selected, type](bool confirmed)
        {
            if (!confirmed)
                return;
            begin_pending(cash_shop_settlement::PendingKind::SLOTS, 0);
            BuyCashInventorySlotsPacket(selected, type).dispatch();
        });
    }

    void UICashShop::slots_purchase_succeeded()
    {
        // The 0x60 handler already raised the tab's slot maximum through
        // Inventory::set_slotmax, and the balances arrive as a separate
        // 0x144 push: settle now, clear_pending()'s refresh repaints the
        // pill labels with the new counts.
        finish_pending();
    }
}
