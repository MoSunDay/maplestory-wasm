#include "UICashShop.h"

#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../KeyAction.h"
#include "../UI.h"
#include "../UITypes/UINotice.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"

namespace jrc
{
    namespace view = cash_shop_view;
    namespace layout = cash_shop_layout;

    namespace
    {
        Point<int16_t> currency_position(size_t index)
        {
            return {
                static_cast<int16_t>(
                    layout::CURRENCY_POS.x() + index * layout::CURRENCY_STEP_X),
                layout::CURRENCY_POS.y()
            };
        }

        Point<int16_t> currency_label_position(size_t index)
        {
            return currency_position(index) +
                Point<int16_t>{
                    static_cast<int16_t>(layout::CURRENCY_SIZE.x() / 2), 3 };
        }

        Point<int16_t> tab_label_position(Point<int16_t> tab_position)
        {
            return tab_position +
                Point<int16_t>{
                    static_cast<int16_t>(layout::TAB_SIZE.x() / 2), 4 };
        }

        Point<int16_t> icon_corner(const Texture& icon, Point<int16_t> center)
        {
            return center - icon.get_dimensions() / 2;
        }
    }

    CashCurrency UICashShop::currency_choice(size_t index)
    {
        switch (index)
        {
        case 1: return CashCurrency::MAPLE_POINT;
        case 2: return CashCurrency::NX_PREPAID;
        default: return CashCurrency::NX_CREDIT;
        }
    }

    const char* UICashShop::currency_short_name(CashCurrency value)
    {
        switch (value)
        {
        case CashCurrency::MAPLE_POINT: return "地图点 ";
        case CashCurrency::NX_PREPAID: return "预付点 ";
        default: return "信用点 ";
        }
    }

    UICashShop::UICashShop(std::shared_ptr<CashShopModel> value, bool is_female)
        : model(std::move(value)), female(is_female),
          right_pane(view::RightPane::LOCKER),
          currency(CashCurrency::NX_CREDIT), category(0), left_page(0),
          right_page(0), pending(false), balance_requested(false), pending_cash_id(0),
          last_cursor_position(),
          skin(cash_shop_skin::load_cash_shop_skin()), screen_scale(1.0f),
          screen_offset(),
          search(Text::A11M, Text::LEFT, Text::DARKGREY,
              layout::SEARCH_FIELD_BOUNDS, 40),
          title(Text::A12B, Text::LEFT, Text::YELLOW, "现金商城"),
          search_label(Text::A11M, Text::LEFT, Text::LIGHTGREY, "搜索:"),
          transfer_hint(Text::A11M, Text::LEFT, Text::LIGHTGREY)
    {
        buttons[BT_EXIT] =
            std::make_unique<MapleButton>(skin.exit_button, layout::EXIT_POS);
        buttons[BT_CHARGE] = std::make_unique<MapleButton>(
            skin.charge_button, layout::CHARGE_POS);
        buttons[BT_BUY] = std::make_unique<MapleButton>(
            skin.buy_button,
            layout::GRID_POS + layout::CARD_BUY_OFFSET);
        buttons[BT_TRANSFER] = std::make_unique<MapleButton>(
            skin.transfer_button, layout::TRANSFER_POS);
        buttons[BT_LEFT_PREV] = std::make_unique<MapleButton>(
            skin.prev_button, layout::LEFT_PREV_POS);
        buttons[BT_LEFT_NEXT] = std::make_unique<MapleButton>(
            skin.next_button, layout::LEFT_NEXT_POS);
        buttons[BT_RIGHT_PREV] = std::make_unique<MapleButton>(
            skin.prev_button, layout::RIGHT_PREV_POS);
        buttons[BT_RIGHT_NEXT] = std::make_unique<MapleButton>(
            skin.next_button, layout::RIGHT_NEXT_POS);

        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
        {
            buttons[BT_CARD_FIRST + card] = std::make_unique<AreaButton>(
                layout::card_position(card), layout::CARD_SIZE);
            card_names[card] = Text(
                Text::A11M, Text::CENTER, Text::BLACK, "",
                layout::CARD_NAME_WIDTH);
        }
        for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
        {
            buttons[BT_SLOT_FIRST + slot] = std::make_unique<AreaButton>(
                layout::slot_position(slot), layout::SLOT_SIZE);
            slot_counts[slot] = Text(Text::A11M, Text::RIGHT, Text::BLACK);
        }
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            buttons[BT_TAB_FIRST + entry] = std::make_unique<AreaButton>(
                layout::category_position(entry), layout::CATEGORY_SIZE);
            category_labels[entry] = Text(
                Text::A11M, Text::LEFT, Text::LIGHTGREY,
                view::category_name(entry));
        }
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
        {
            buttons[BT_CURRENCY_FIRST + choice] = std::make_unique<AreaButton>(
                currency_position(choice), layout::CURRENCY_SIZE);
            balance_labels[choice] = Text(
                Text::A11M, Text::LEFT, Text::LIGHTGREY,
                currency_short_name(currency_choice(choice)));
            currency_labels[choice] = Text(
                Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
                layout::CURRENCY_SIZE.x());
        }
        buttons[BT_RIGHT_LOCKER_TAB] = std::make_unique<AreaButton>(
            layout::TAB_LOCKER_POS, layout::TAB_SIZE);
        buttons[BT_RIGHT_INVENTORY_TAB] = std::make_unique<AreaButton>(
            layout::TAB_INVENTORY_POS, layout::TAB_SIZE);
        tab_labels[0] = Text(
            Text::A11M, Text::CENTER, Text::BLACK, view::pane_name(
                view::RightPane::LOCKER), layout::TAB_LABEL_WIDTH);
        tab_labels[1] = Text(
            Text::A11M, Text::CENTER, Text::WHITE, view::pane_name(
                view::RightPane::INVENTORY), layout::TAB_LABEL_WIDTH);
        page_labels[0] = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::LEFT_PAGE_WIDTH);
        page_labels[1] = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::RIGHT_PAGE_WIDTH);

        search.set_enter_callback([this](const std::string&) { rebuild_catalog(); });
        update_screen(Constants::viewwidth(), Constants::viewheight());
        rebuild_catalog();
    }

    void UICashShop::draw(float alpha) const
    {
        const auto stretched = [this](Point<int16_t> pos, Point<int16_t> size) {
            return DrawArgument(
                map_to_screen(pos),
                layout::to_screen_size(size, screen_scale));
        };

        skin.background.draw({ {}, dimension });
        title.draw(map_to_screen(layout::TITLE_POS));

        const CashBalances& balances = model->balances();
        const int32_t balance_values[CURRENCY_CHOICES] = {
            balances.nx_credit, balances.maple_points, balances.nx_prepaid
        };
        for (size_t row = 0; row < CURRENCY_CHOICES; ++row)
        {
            const Point<int16_t> label{ layout::BALANCE_LABEL_X,
                layout::BALANCE_ROWS_Y[row] };
            const Point<int16_t> value{ layout::BALANCE_VALUE_RIGHT_X,
                layout::BALANCE_ROWS_Y[row] };
            balance_labels[row].draw(map_to_screen(label));
            cash_shop_skin::draw_amount_right(
                skin, map_to_screen(value), balance_values[row]);
        }

        skin.sort_bar.draw(
            stretched(layout::SEARCH_BAR_POS, layout::SEARCH_BAR_SIZE));
        search_label.draw(map_to_screen(layout::SEARCH_LABEL_POS));
        search.draw({});
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            currency_labels[choice].draw(
                map_to_screen(currency_label_position(choice)));

        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            const Point<int16_t> base = layout::category_position(entry);
            category_labels[entry].draw(
                map_to_screen(base + layout::CATEGORY_LABEL_OFFSET));
            if (entry == category)
                skin.line.draw(stretched(
                    base + layout::CATEGORY_LINE_OFFSET,
                    layout::CATEGORY_LINE_SIZE));
        }

        const auto left_range =
            view::page_range(left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        if (left_range.first == left_range.second)
        {
            skin.no_item.draw(stretched(
                layout::EMPTY_CATALOG_CENTER - layout::EMPTY_HINT_SIZE / 2,
                layout::EMPTY_HINT_SIZE));
        }
        else
        {
            for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
            {
                const size_t index = left_range.first + card;
                const Point<int16_t> base = layout::card_position(card);
                skin.card.draw(stretched(base, layout::CARD_SIZE));
                if (index >= left_range.second)
                    continue;
                const CashCommodity& item =
                    model->catalog()[filtered[index]];
                if (const ItemData& data = ItemData::get(item.item_id))
                {
                    const Texture& icon = data.get_icon(false);
                    icon.draw({ map_to_screen(
                        base + layout::CARD_ICON_CENTER -
                        icon.get_dimensions() / 2) });
                }
                card_names[card].draw(
                    map_to_screen(base + layout::CARD_NAME_POS));
                const Point<int16_t> price = map_to_screen(
                    base + layout::CARD_PRICE_ICON_POS);
                skin.nx_icon.draw({ price });
                cash_shop_skin::draw_amount(
                    skin,
                    price + Point<int16_t>{ skin.nx_icon.width() + 2, 0 },
                    item.price);
                if (selected_left && *selected_left == index)
                    skin.selected_marker.draw({ map_to_screen(
                        base + layout::CARD_MARKER_OFFSET) });
            }
        }
        page_labels[0].draw(map_to_screen(layout::LEFT_PAGE_POS));

        const bool locker_active = right_pane == view::RightPane::LOCKER;
        (locker_active ? skin.tab_enabled : skin.tab_disabled).draw(
            stretched(layout::TAB_LOCKER_POS, layout::TAB_SIZE));
        (locker_active ? skin.tab_disabled : skin.tab_enabled).draw(
            stretched(layout::TAB_INVENTORY_POS, layout::TAB_SIZE));
        tab_labels[0].draw(
            map_to_screen(tab_label_position(layout::TAB_LOCKER_POS)));
        tab_labels[1].draw(
            map_to_screen(tab_label_position(layout::TAB_INVENTORY_POS)));

        skin.inventory_panel.draw(
            stretched(layout::PANEL_POS, layout::PANEL_SIZE));
        const auto right_range = view::page_range(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        if (right_range.first == right_range.second)
        {
            skin.no_item.draw(stretched(
                layout::EMPTY_RIGHT_CENTER - layout::EMPTY_HINT_SIZE / 2,
                layout::EMPTY_HINT_SIZE));
        }
        else
        {
            for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
            {
                const size_t index = right_range.first + slot;
                if (index >= right_range.second)
                    continue;
                const Point<int16_t> base = layout::slot_position(slot);
                if (const ItemData& data = ItemData::get(right_item_id(index)))
                {
                    const Texture& icon = data.get_icon(false);
                    icon.draw({ map_to_screen(icon_corner(
                        icon, base + layout::SLOT_SIZE / 2)) });
                }
                slot_counts[slot].draw(
                    map_to_screen(base + layout::SLOT_COUNT_OFFSET));
                if (selected_right && *selected_right == index)
                    skin.selected_marker.draw({ map_to_screen(
                        base + layout::SLOT_MARKER_OFFSET) });
            }
        }
        transfer_hint.draw(map_to_screen(layout::TRANSFER_HINT_POS));
        page_labels[1].draw(map_to_screen(layout::RIGHT_PAGE_POS));

        UIElement::draw_buttons(alpha);
    }

    void UICashShop::update()
    {
        UIElement::update();
        search.update({});
        if (!balance_requested)
        {
            balance_requested = true;
            CheckCashPacket().dispatch();
        }
    }

    void UICashShop::update_screen(int16_t width, int16_t height)
    {
        position = {};
        dimension = { width, height };
        screen_scale = layout::content_scale(width, height);
        screen_offset = layout::content_offset(width, height);
        position_static_buttons();
        position_buy_button();
    }

    Point<int16_t> UICashShop::map_to_screen(Point<int16_t> design) const
    {
        return layout::to_screen(design, screen_scale, screen_offset);
    }

    void UICashShop::position_static_buttons()
    {
        buttons[BT_EXIT]->set_position(map_to_screen(layout::EXIT_POS));
        buttons[BT_CHARGE]->set_position(map_to_screen(layout::CHARGE_POS));
        buttons[BT_TRANSFER]->set_position(
            map_to_screen(layout::TRANSFER_POS));
        buttons[BT_LEFT_PREV]->set_position(
            map_to_screen(layout::LEFT_PREV_POS));
        buttons[BT_LEFT_NEXT]->set_position(
            map_to_screen(layout::LEFT_NEXT_POS));
        buttons[BT_RIGHT_PREV]->set_position(
            map_to_screen(layout::RIGHT_PREV_POS));
        buttons[BT_RIGHT_NEXT]->set_position(
            map_to_screen(layout::RIGHT_NEXT_POS));
        buttons[BT_RIGHT_LOCKER_TAB]->set_position(
            map_to_screen(layout::TAB_LOCKER_POS));
        buttons[BT_RIGHT_INVENTORY_TAB]->set_position(
            map_to_screen(layout::TAB_INVENTORY_POS));
        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
            buttons[BT_CARD_FIRST + card]->set_position(
                map_to_screen(layout::card_position(card)));
        for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
            buttons[BT_SLOT_FIRST + slot]->set_position(
                map_to_screen(layout::slot_position(slot)));
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
            buttons[BT_TAB_FIRST + entry]->set_position(
                map_to_screen(layout::category_position(entry)));
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
            buttons[BT_CURRENCY_FIRST + choice]->set_position(
                map_to_screen(currency_position(choice)));
    }

    void UICashShop::position_buy_button()
    {
        if (!selected_left)
            return;
        buttons[BT_BUY]->set_position(map_to_screen(
            layout::card_position(*selected_left % view::CATALOG_PAGE_SIZE) +
            layout::CARD_BUY_OFFSET));
    }

    UIElement::CursorResult UICashShop::send_cursor(
        bool pressed, Point<int16_t> cursorpos)
    {
        last_cursor_position =
            layout::to_design(cursorpos, screen_scale, screen_offset);
        if (layout::SEARCH_HITBOX.contains(last_cursor_position))
        {
            if (pressed)
                search.set_state(Textfield::FOCUSED);
            return { pressed ? Cursor::CLICKING : Cursor::CANCLICK, true };
        }
        if (pressed && search.get_state() == Textfield::FOCUSED)
            search.set_state(Textfield::NORMAL);
        return UIElement::send_cursor(pressed, cursorpos);
    }

    void UICashShop::send_scroll(double yoffset)
    {
        const bool over_catalog = last_cursor_position.x() < 500;
        size_t& page = over_catalog ? left_page : right_page;
        const size_t count = over_catalog ? filtered.size() : right_count();
        const size_t page_size = over_catalog ? view::CATALOG_PAGE_SIZE :
            view::RIGHT_PAGE_SIZE;
        if (yoffset > 0 && page > 0)
            --page;
        else if (yoffset < 0 && page + 1 < view::page_count(count, page_size))
            ++page;
        refresh();
    }

    void UICashShop::send_key(int32_t keycode, bool pressed, bool escape)
    {
        if (pressed && escape && !pending)
            button_pressed(BT_EXIT);
        else if (pressed && keycode == KeyAction::RETURN && selected_left && !pending)
            request_purchase(*selected_left);
    }

    Button::State UICashShop::button_pressed(uint16_t id)
    {
        if (pending)
            return Button::DISABLED;
        if (id == BT_EXIT)
        {
            pending = true;
            UI::get().disable();
            LeaveCashShopPacket().dispatch();
        }
        else if (id == BT_CHARGE)
        {
            UI::get().emplace<UIOk>("当前版本暂不支持充值。", []() {});
        }
        else if (id == BT_BUY && selected_left)
            request_purchase(*selected_left);
        else if (id == BT_TRANSFER && selected_right)
            request_transfer(*selected_right);
        else if (id >= BT_TAB_FIRST && id < BT_TAB_LAST)
        {
            const int8_t entry = static_cast<int8_t>(id - BT_TAB_FIRST);
            if (entry != category)
            {
                category = entry;
                rebuild_catalog();
            }
        }
        else if (id >= BT_CURRENCY_FIRST && id < BT_CURRENCY_LAST)
        {
            const CashCurrency choice =
                currency_choice(id - BT_CURRENCY_FIRST);
            if (choice != currency)
            {
                currency = choice;
                refresh();
            }
        }
        else if (id == BT_RIGHT_LOCKER_TAB)
            change_right_pane(view::RightPane::LOCKER);
        else if (id == BT_RIGHT_INVENTORY_TAB)
            change_right_pane(view::RightPane::INVENTORY);
        else if (id == BT_LEFT_PREV && left_page > 0)
            --left_page;
        else if (id == BT_LEFT_NEXT &&
            left_page + 1 < view::page_count(filtered.size(),
                view::CATALOG_PAGE_SIZE))
            ++left_page;
        else if (id == BT_RIGHT_PREV && right_page > 0)
            --right_page;
        else if (id == BT_RIGHT_NEXT &&
            right_page + 1 < view::page_count(right_count(),
                view::RIGHT_PAGE_SIZE))
            ++right_page;
        else if (id >= BT_CARD_FIRST && id < BT_CARD_LAST)
        {
            const size_t index = left_page * view::CATALOG_PAGE_SIZE +
                (id - BT_CARD_FIRST);
            if (index < filtered.size())
            {
                if (selected_left && *selected_left == index)
                    request_purchase(index);
                else
                    selected_left = index;
            }
        }
        else if (id >= BT_SLOT_FIRST && id < BT_SLOT_LAST)
        {
            const size_t index = right_page * view::RIGHT_PAGE_SIZE +
                (id - BT_SLOT_FIRST);
            if (index < right_count())
            {
                if (selected_right && *selected_right == index)
                    request_transfer(index);
                else
                    selected_right = index;
            }
        }
        refresh();
        return Button::NORMAL;
    }
}
