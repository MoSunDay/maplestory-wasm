#include "UICashShopButtons.h"

#include "CashShopLayout.h"
#include "../Components/AreaButton.h"

#include <vector>

namespace jrc
{
    namespace
    {
        namespace layout = cash_shop_layout;

        // Design-space hit area of one button id.
        struct HitArea
        {
            Point<int16_t> position;
            Point<int16_t> size;
        };

        // Every button id the cash shop owns, singles followed by ranges.
        std::vector<uint16_t> button_ids()
        {
            std::vector<uint16_t> ids = {
                BT_EXIT, BT_CHARGE, BT_BUY, BT_RESET, BT_TRANSFER,
                BT_LEFT_PREV, BT_LEFT_NEXT, BT_RIGHT_PREV, BT_RIGHT_NEXT,
                BT_FEATURED, BT_EQSUB_CLOSE,
                BT_RIGHT_LOCKER_TAB, BT_RIGHT_INVENTORY_TAB
            };
            const auto append = [&ids](uint16_t first, uint16_t last)
            {
                for (uint16_t id = first; id < last; ++id)
                    ids.push_back(id);
            };
            append(BT_CARD_FIRST, BT_CARD_LAST);
            append(BT_SLOT_FIRST, BT_SLOT_LAST);
            append(BT_TAB_FIRST, BT_TAB_LAST);
            append(BT_EQSUB_FIRST, BT_EQSUB_LAST);
            append(BT_CURRENCY_FIRST, BT_CURRENCY_LAST);
            return ids;
        }

        // Design-space hit area of a button id; false for unknown ids.
        bool button_geometry(uint16_t id, HitArea& area)
        {
            const auto place = [&](Point<int16_t> position,
                Point<int16_t> size)
            {
                area = { position, size };
                return true;
            };

            switch (id)
            {
            case BT_EXIT:
                return place(layout::EXIT_POS, layout::EXIT_SIZE);
            case BT_CHARGE:
                return place(layout::CHARGE_POS, layout::CHARGE_SIZE);
            case BT_BUY:
                return place(layout::card_position(0) +
                    layout::CARD_BUY_OFFSET, layout::CARD_BUY_SIZE);
            case BT_RESET:
                return place(layout::RESET_POS, layout::RESET_SIZE);
            case BT_TRANSFER:
                return place(layout::TRANSFER_POS, layout::TRANSFER_SIZE);
            case BT_LEFT_PREV:
                return place(layout::LEFT_PREV_POS, layout::PAGER_SIZE);
            case BT_LEFT_NEXT:
                return place(layout::LEFT_NEXT_POS, layout::PAGER_SIZE);
            case BT_RIGHT_PREV:
                return place(layout::RIGHT_PREV_POS, layout::PAGER_SIZE);
            case BT_RIGHT_NEXT:
                return place(layout::RIGHT_NEXT_POS, layout::PAGER_SIZE);
            case BT_FEATURED:
                return place(layout::FEATURED_PANEL_POS,
                    layout::FEATURED_PANEL_SIZE);
            case BT_EQSUB_CLOSE:
                return place(layout::EQSUB_CLOSE_POS,
                    layout::EQSUB_CLOSE_SIZE);
            case BT_RIGHT_LOCKER_TAB:
                return place(layout::TAB_LOCKER_POS, layout::TAB_SIZE);
            case BT_RIGHT_INVENTORY_TAB:
                return place(layout::TAB_INVENTORY_POS, layout::TAB_SIZE);
            default:
                break;
            }
            if (id >= BT_CARD_FIRST && id < BT_CARD_LAST)
                return place(layout::card_position(
                    static_cast<size_t>(id - BT_CARD_FIRST)),
                    layout::CARD_SIZE);
            if (id >= BT_SLOT_FIRST && id < BT_SLOT_LAST)
                return place(layout::slot_position(
                    static_cast<size_t>(id - BT_SLOT_FIRST)),
                    layout::SLOT_SIZE);
            if (id >= BT_TAB_FIRST && id < BT_TAB_LAST)
                return place(layout::category_position(
                    static_cast<int8_t>(id - BT_TAB_FIRST)),
                    layout::CATEGORY_SIZE);
            if (id >= BT_EQSUB_FIRST && id < BT_EQSUB_LAST)
                return place(layout::equip_sub_position(
                    static_cast<int8_t>(id - BT_EQSUB_FIRST)),
                    layout::EQSUB_ENTRY_SIZE);
            if (id >= BT_CURRENCY_FIRST && id < BT_CURRENCY_LAST)
                return place(layout::currency_position(
                    static_cast<size_t>(id - BT_CURRENCY_FIRST)),
                    layout::CURRENCY_SIZE);
            return false;
        }
    }

    void cash_shop_buttons::apply_geometry(
        std::map<uint16_t, std::unique_ptr<Button>>& buttons)
    {
        for (uint16_t id : button_ids())
        {
            HitArea area;
            if (!button_geometry(id, area))
                continue;
            const Point<int16_t> position =
                layout::scale_point(area.position);
            const Point<int16_t> size = layout::scale_point(area.size);
            const auto found = buttons.find(id);
            if (found == buttons.end())
            {
                buttons.emplace(
                    id, std::make_unique<AreaButton>(position, size));
                continue;
            }
            // Only AreaButtons carry a scalable dimension; other button
            // types keep their own artwork-driven geometry.
            auto* area_button =
                dynamic_cast<AreaButton*>(found->second.get());
            if (!area_button)
                continue;
            area_button->set_position(position);
            area_button->set_dimension(size);
        }
    }
}
