#include "UICashShop.h"

#include "../Components/AreaButton.h"
#include "../KeyAction.h"
#include "../UI.h"
#include "../UITypes/UINotice.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/CashShopPackets.h"

#include <algorithm>

namespace jrc
{
    namespace view = cash_shop_view;
    namespace layout = cash_shop_layout;

    namespace
    {
        // The scene is rendered with Constants::VIEWYOFFSET applied, while
        // hitboxes and layout constants use the final canvas coordinates.
        // Every drawing call therefore subtracts the offset once here.
        Point<int16_t> draw_position(Point<int16_t> position)
        {
            return position - Point<int16_t>{ 0, Constants::VIEWYOFFSET };
        }

        void fill_rect(Point<int16_t> position, Point<int16_t> size,
            float r, float g, float b, float a)
        {
            const Point<int16_t> drawn = draw_position(position);
            GraphicsGL::get().drawrectangle(
                drawn.x(), drawn.y(), size.x(), size.y(), r, g, b, a);
        }

        void outline_rect(Point<int16_t> position, Point<int16_t> size,
            float r, float g, float b, float a, int16_t thickness = 1)
        {
            const int16_t width = size.x();
            const int16_t height = size.y();
            fill_rect(position, { width, thickness }, r, g, b, a);
            fill_rect(
                { position.x(),
                    static_cast<int16_t>(
                        position.y() + height - thickness) },
                { width, thickness }, r, g, b, a);
            fill_rect(position, { thickness, height }, r, g, b, a);
            fill_rect(
                { static_cast<int16_t>(
                    position.x() + width - thickness), position.y() },
                { thickness, height }, r, g, b, a);
        }

        Point<int16_t> currency_label_position(size_t index)
        {
            return layout::pill_label(
                layout::currency_position(index), layout::CURRENCY_SIZE);
        }

        Point<int16_t> tab_label_position(Point<int16_t> tab_position)
        {
            return layout::pill_label(tab_position, layout::TAB_SIZE);
        }

        size_t button_frame(Button::State state)
        {
            switch (state)
            {
            case Button::PRESSED: return 1;
            case Button::MOUSEOVER: return 2;
            case Button::DISABLED: return 3;
            default: return 0;
            }
        }
    }

    UICashShop::UICashShop(std::shared_ptr<CashShopModel> value, bool is_female)
        : model(std::move(value)), female(is_female),
          right_pane(view::RightPane::LOCKER),
          currency(CashCurrency::NX_CREDIT), category(0), left_page(0),
          right_page(0), pending(false), balance_requested(false), pending_cash_id(0),
          last_cursor_position(),
          skin(cash_shop_skin::load_cash_shop_skin()),
          search(Text::A11M, Text::LEFT, Text::WHITE,
              layout::SEARCH_FIELD_BOUNDS, 40),
          title(Text::A12B, Text::LEFT, Text::YELLOW, "现金商城"),
          category_title(Text::A12B, Text::LEFT, Text::WHITE, "分类"),
          pane_title(Text::A12B, Text::LEFT, Text::WHITE),
          pane_info(Text::A11M, Text::RIGHT, Text::LIGHTGREY),
          search_label(Text::A11M, Text::LEFT, Text::LIGHTGREY, "搜索:"),
          transfer_hint(Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
              layout::TRANSFER_HINT_WIDTH),
          details_hint(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "选择物品查看详情", layout::DETAILS_HINT_WIDTH),
          details_name(Text::A11B, Text::CENTER, Text::WHITE, "",
              layout::DETAILS_TEXT_WIDTH),
          details_count(Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
              layout::DETAILS_TEXT_WIDTH),
          buy_label(Text::A11M, Text::CENTER, Text::BLACK, "购买"),
          transfer_label(Text::A11M, Text::CENTER, Text::WHITE, "存入"),
          charge_label(Text::A11M, Text::CENTER, Text::BLACK, "充值"),
          exit_label(Text::A11M, Text::CENTER, Text::WHITE, "关闭")
    {
        buttons[BT_EXIT] = std::make_unique<AreaButton>(
            layout::EXIT_POS, layout::EXIT_SIZE);
        buttons[BT_CHARGE] = std::make_unique<AreaButton>(
            layout::CHARGE_POS, layout::CHARGE_SIZE);
        buttons[BT_BUY] = std::make_unique<AreaButton>(
            layout::card_position(0) + layout::CARD_BUY_OFFSET,
            layout::CARD_BUY_SIZE);
        buttons[BT_TRANSFER] = std::make_unique<AreaButton>(
            layout::TRANSFER_POS, layout::TRANSFER_SIZE);
        buttons[BT_LEFT_PREV] = std::make_unique<AreaButton>(
            layout::LEFT_PREV_POS, layout::PAGER_SIZE);
        buttons[BT_LEFT_NEXT] = std::make_unique<AreaButton>(
            layout::LEFT_NEXT_POS, layout::PAGER_SIZE);
        buttons[BT_RIGHT_PREV] = std::make_unique<AreaButton>(
            layout::RIGHT_PREV_POS, layout::PAGER_SIZE);
        buttons[BT_RIGHT_NEXT] = std::make_unique<AreaButton>(
            layout::RIGHT_NEXT_POS, layout::PAGER_SIZE);

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
                Text::A11M, Text::LEFT, Text::DARKGREY,
                view::category_name(entry));
        }
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
        {
            buttons[BT_CURRENCY_FIRST + choice] = std::make_unique<AreaButton>(
                layout::currency_position(choice), layout::CURRENCY_SIZE);
            balance_labels[choice] = Text(
                Text::A11M, Text::LEFT, Text::LIGHTGREY,
                currency_short_name(currency_choice(choice)));
            currency_labels[choice] = Text(
                Text::A11M, Text::CENTER, Text::LIGHTGREY,
                currency_short_name(currency_choice(choice)),
                layout::CURRENCY_SIZE.x());
        }
        buttons[BT_RIGHT_LOCKER_TAB] = std::make_unique<AreaButton>(
            layout::TAB_LOCKER_POS, layout::TAB_SIZE);
        buttons[BT_RIGHT_INVENTORY_TAB] = std::make_unique<AreaButton>(
            layout::TAB_INVENTORY_POS, layout::TAB_SIZE);
        tab_labels[0] = Text(
            Text::A11M, Text::CENTER, Text::WHITE, view::pane_name(
                view::RightPane::LOCKER), layout::TAB_LABEL_WIDTH);
        tab_labels[1] = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, view::pane_name(
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
        (void)alpha;
        skin.background.draw({ draw_position({}), dimension });

        const auto state_of = [this](uint16_t id) -> Button::State
        {
            const auto found = buttons.find(id);
            return found != buttons.end() ? found->second->get_state() :
                Button::NORMAL;
        };
        // Dark header band over the painted artwork.
        fill_rect(layout::TOP_BAR_POS, layout::TOP_BAR_SIZE,
            0.04f, 0.05f, 0.07f, 0.92f);
        fill_rect(
            { layout::TOP_BAR_POS.x(),
                static_cast<int16_t>(layout::TOP_BAR_POS.y() +
                    layout::TOP_BAR_SIZE.y() - 1) },
            { layout::TOP_BAR_SIZE.x(), 1 }, 0.88f, 0.52f, 0.16f, 0.35f);

        title.draw(draw_position(layout::TITLE_POS));
        category_title.draw(draw_position(layout::CATEGORY_TITLE_POS));

        const CashBalances& balances = model->balances();
        const int32_t balance_values[CURRENCY_CHOICES] = {
            balances.nx_credit, balances.maple_points, balances.nx_prepaid
        };
        for (size_t row = 0; row < CURRENCY_CHOICES; ++row)
        {
            balance_labels[row].draw(draw_position(
                { layout::BALANCE_LABEL_XS[row],
                    static_cast<int16_t>(layout::BALANCE_Y -
                        layout::TEXT_BASELINE_SHIFT) }));
            cash_shop_skin::draw_amount_right(
                skin,
                draw_position(
                    { layout::BALANCE_VALUE_RIGHT_XS[row], layout::BALANCE_Y }),
                balance_values[row]);
        }

        // Custom dark search bar with a warm accent on the leading edge.
        fill_rect(layout::SEARCH_BAR_POS, layout::SEARCH_BAR_SIZE,
            0.08f, 0.09f, 0.11f, 0.96f);
        outline_rect(layout::SEARCH_BAR_POS, layout::SEARCH_BAR_SIZE,
            0.25f, 0.28f, 0.34f, 1.0f);
        fill_rect(layout::SEARCH_BAR_POS,
            { 3, layout::SEARCH_BAR_SIZE.y() },
            0.88f, 0.52f, 0.16f, 1.0f);
        if (search.get_state() == Textfield::FOCUSED)
            outline_rect(layout::SEARCH_BAR_POS, layout::SEARCH_BAR_SIZE,
                1.0f, 0.62f, 0.22f, 1.0f);
        search_label.draw(draw_position(layout::SEARCH_LABEL_POS));
        // Bounds are canvas-space: pass a zero parent so the IME bridge receives
        // them verbatim (Textfield::draw adds the parent to its stored bounds).
        search.draw(Point<int16_t>{});

        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
        {
            const Point<int16_t> base = layout::currency_position(choice);
            const bool selected = currency == currency_choice(choice);
            const bool hovered = !selected &&
                state_of(BT_CURRENCY_FIRST + choice) == Button::MOUSEOVER;
            if (selected)
            {
                fill_rect(base, layout::CURRENCY_SIZE,
                    0.18f, 0.13f, 0.05f, 0.95f);
                fill_rect(
                    { base.x(), static_cast<int16_t>(base.y() +
                        layout::CURRENCY_SIZE.y() - 2) },
                    { layout::CURRENCY_SIZE.x(), 2 },
                    0.88f, 0.52f, 0.16f, 1.0f);
                outline_rect(base, layout::CURRENCY_SIZE,
                    0.88f, 0.52f, 0.16f, 1.0f);
            }
            else
            {
                const float tint = hovered ? 0.11f : 0.07f;
                fill_rect(base, layout::CURRENCY_SIZE,
                    tint, tint + 0.01f, tint + 0.02f, 0.85f);
                outline_rect(base, layout::CURRENCY_SIZE,
                    0.25f, 0.28f, 0.34f, 1.0f);
            }
            currency_labels[choice].draw(
                draw_position(currency_label_position(choice)));
        }

        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            const Point<int16_t> base = layout::category_position(entry);
            if (entry == category)
            {
                fill_rect(base, layout::CATEGORY_SIZE,
                    0.02f, 0.03f, 0.05f, 0.08f);
                fill_rect(base + layout::CATEGORY_LINE_OFFSET,
                    layout::CATEGORY_LINE_SIZE,
                    0.88f, 0.52f, 0.16f, 1.0f);
            }
            category_labels[entry].draw(
                draw_position(base + layout::CATEGORY_LABEL_OFFSET));
        }

        const auto left_range =
            view::page_range(left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        if (left_range.first == left_range.second)
        {
            skin.no_item.draw({ draw_position(layout::EMPTY_CATALOG_CENTER -
                layout::EMPTY_HINT_SIZE / 2), layout::EMPTY_HINT_SIZE });
        }
        else
        {
            for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
            {
                const size_t index = left_range.first + card;
                const Point<int16_t> base = layout::card_position(card);
                skin.card.draw({ draw_position(base), layout::CARD_SIZE });
                const bool active = index < left_range.second;
                bool selected = false;
                bool hovered = false;
                if (active)
                {
                    const CashCommodity& item =
                        model->catalog()[filtered[index]];
                    if (const ItemData& data = ItemData::get(item.item_id))
                        draw_icon(data.get_icon(false),
                            base + layout::CARD_ICON_CENTER,
                            layout::CARD_ICON_MAX);
                    card_names[card].draw(
                        draw_position(base + layout::CARD_NAME_POS));
                    const Point<int16_t> price =
                        base + layout::CARD_PRICE_ICON_POS;
                    skin.nx_icon.draw(draw_position(price));
                    cash_shop_skin::draw_amount(
                        skin,
                        draw_position(price + Point<int16_t>{
                            skin.nx_icon.width() + 2, 0 }),
                        item.price, Color::BLACK);
                    selected = selected_left && *selected_left == index;
                    hovered = !selected &&
                        state_of(BT_CARD_FIRST + card) == Button::MOUSEOVER;
                    if (selected)
                        draw_icon(skin.selected_marker,
                            base + layout::CARD_MARKER_CENTER, layout::px(12));
                }
                if (selected)
                    outline_rect(base, layout::CARD_SIZE,
                        0.88f, 0.52f, 0.16f, 1.0f, 2);
                else if (hovered)
                    outline_rect(base, layout::CARD_SIZE,
                        0.78f, 0.81f, 0.85f, 0.85f, 2);
            }
        }
        page_labels[0].draw(draw_position(layout::LEFT_PAGE_POS));

        const auto flat_tab = [&](uint16_t id, Point<int16_t> tab_position,
            bool selected)
        {
            const bool hovered = !selected &&
                state_of(id) == Button::MOUSEOVER;
            if (selected)
                fill_rect(tab_position, layout::TAB_SIZE,
                    0.20f, 0.17f, 0.11f, 1.0f);
            else if (hovered)
                fill_rect(tab_position, layout::TAB_SIZE,
                    0.14f, 0.15f, 0.18f, 1.0f);
            else
                fill_rect(tab_position, layout::TAB_SIZE,
                    0.10f, 0.11f, 0.135f, 1.0f);
            outline_rect(tab_position, layout::TAB_SIZE,
                0.25f, 0.28f, 0.34f, 1.0f);
            if (selected)
                fill_rect(
                    { tab_position.x(), static_cast<int16_t>(tab_position.y() +
                        layout::TAB_SIZE.y() - 2) },
                    { layout::TAB_SIZE.x(), 2 },
                    0.88f, 0.52f, 0.16f, 1.0f);
        };

        const bool locker_active = right_pane == view::RightPane::LOCKER;
        flat_tab(BT_RIGHT_LOCKER_TAB, layout::TAB_LOCKER_POS, locker_active);
        flat_tab(BT_RIGHT_INVENTORY_TAB, layout::TAB_INVENTORY_POS,
            !locker_active);
        skin.tab_bar.draw(
            { draw_position(layout::TAB_BAR_POS), layout::TAB_BAR_SIZE });
        tab_labels[0].draw(
            draw_position(tab_label_position(layout::TAB_LOCKER_POS)));
        tab_labels[1].draw(
            draw_position(tab_label_position(layout::TAB_INVENTORY_POS)));

        skin.inventory_panel.draw(
            { draw_position(layout::PANEL_POS), layout::PANEL_SIZE });

        // Replace the painted Korean panel header with a flat title band.
        const int16_t header_height = static_cast<int16_t>(
            layout::SLOT_GRID_POS.y() - layout::PANEL_POS.y());
        fill_rect(layout::PANEL_POS,
            { layout::PANEL_SIZE.x(), header_height },
            0.06f, 0.07f, 0.09f, 1.0f);
        fill_rect(layout::PANEL_POS, { layout::PANEL_SIZE.x(), 1 },
            0.25f, 0.28f, 0.34f, 1.0f);
        fill_rect(
            { layout::PANEL_POS.x(),
                static_cast<int16_t>(layout::SLOT_GRID_POS.y() - 1) },
            { layout::PANEL_SIZE.x(), 1 }, 0.25f, 0.28f, 0.34f, 1.0f);
        pane_title.draw(draw_position(layout::PANE_TITLE_POS));
        pane_info.draw(draw_position(layout::PANE_INFO_RIGHT));

        const auto right_range = view::page_range(
            right_page, right_count(), view::RIGHT_PAGE_SIZE);
        if (right_range.first == right_range.second)
        {
            skin.no_item.draw({ draw_position(layout::EMPTY_RIGHT_CENTER -
                layout::EMPTY_HINT_SIZE / 2), layout::EMPTY_HINT_SIZE });
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
                    draw_icon(data.get_icon(false),
                        base + layout::SLOT_SIZE / 2, layout::SLOT_ICON_MAX);
                slot_counts[slot].draw(
                    draw_position(base + layout::SLOT_COUNT_OFFSET));
                const bool selected = selected_right &&
                    *selected_right == index;
                if (selected)
                    draw_icon(skin.selected_marker,
                        base + layout::SLOT_MARKER_CENTER, layout::px(12));
                if (selected)
                    outline_rect(base, layout::SLOT_SIZE,
                        0.88f, 0.52f, 0.16f, 1.0f, 2);
                else if (state_of(BT_SLOT_FIRST + slot) == Button::MOUSEOVER)
                    outline_rect(base, layout::SLOT_SIZE,
                        0.78f, 0.81f, 0.85f, 0.85f, 2);
            }
        }
        transfer_hint.draw(draw_position(layout::TRANSFER_HINT_POS));
        page_labels[1].draw(draw_position(layout::RIGHT_PAGE_POS));

        // Selected item details.
        fill_rect(layout::DETAILS_PANEL_POS, layout::DETAILS_PANEL_SIZE,
            0.06f, 0.07f, 0.09f, 0.90f);
        outline_rect(layout::DETAILS_PANEL_POS, layout::DETAILS_PANEL_SIZE,
            0.25f, 0.28f, 0.34f, 1.0f);
        fill_rect(layout::DETAILS_PANEL_POS, { 64, 2 },
            0.88f, 0.52f, 0.16f, 1.0f);
        const bool details_visible = selected_right &&
            *selected_right / view::RIGHT_PAGE_SIZE == right_page &&
            *selected_right < right_count();
        if (details_visible)
        {
            if (const ItemData& data = ItemData::get(
                    right_item_id(*selected_right)))
                draw_icon(data.get_icon(false), layout::DETAILS_ICON_CENTER,
                    layout::DETAILS_ICON_MAX);
            details_name.draw(draw_position(layout::DETAILS_TEXT_CENTER));
            details_count.draw(draw_position(
                { layout::DETAILS_TEXT_CENTER.x(), layout::DETAILS_COUNT_Y }));
        }
        else
        {
            details_hint.draw(draw_position(layout::DETAILS_HINT_CENTER));
        }

        // Flat action buttons drawn on top of the (unused) native art.
        const auto flat_button = [&](uint16_t id, Point<int16_t> pos,
            Point<int16_t> size, const Text& label,
            float r, float g, float b)
        {
            const auto found = buttons.find(id);
            const bool active = found != buttons.end() &&
                found->second->is_active();
            const Button::State state = found != buttons.end() ?
                found->second->get_state() : Button::NORMAL;
            if (!active)
            {
                fill_rect(pos, size, 0.14f, 0.15f, 0.18f, 0.96f);
                outline_rect(pos, size, 0.25f, 0.28f, 0.34f, 1.0f);
            }
            else
            {
                const float shade = state == Button::PRESSED ? 0.82f :
                    state == Button::MOUSEOVER ? 1.16f : 1.0f;
                const float lit_r = std::min(1.0f, r * shade);
                const float lit_g = std::min(1.0f, g * shade);
                const float lit_b = std::min(1.0f, b * shade);
                fill_rect(pos, size, lit_r, lit_g, lit_b, 0.98f);
                outline_rect(pos, size,
                    std::min(1.0f, lit_r + 0.10f),
                    std::min(1.0f, lit_g + 0.10f),
                    std::min(1.0f, lit_b + 0.10f), 1.0f);
            }
            label.draw(draw_position(layout::pill_label(pos, size)));
        };

        flat_button(BT_CHARGE, layout::CHARGE_POS, layout::CHARGE_SIZE,
            charge_label, 0.85f, 0.68f, 0.20f);
        flat_button(BT_EXIT, layout::EXIT_POS, layout::EXIT_SIZE,
            exit_label, 0.12f, 0.13f, 0.16f);
        if (selected_left &&
            *selected_left / view::CATALOG_PAGE_SIZE == left_page)
            flat_button(BT_BUY,
                layout::card_position(
                    *selected_left % view::CATALOG_PAGE_SIZE) +
                    layout::CARD_BUY_OFFSET,
                layout::CARD_BUY_SIZE, buy_label, 0.88f, 0.52f, 0.16f);
        flat_button(BT_TRANSFER, layout::TRANSFER_POS, layout::TRANSFER_SIZE,
            transfer_label, 0.20f, 0.42f, 0.62f);

        draw_action(BT_LEFT_PREV, skin.prev_button,
            layout::LEFT_PREV_POS, layout::PAGER_SIZE);
        draw_action(BT_LEFT_NEXT, skin.next_button,
            layout::LEFT_NEXT_POS, layout::PAGER_SIZE);
        draw_action(BT_RIGHT_PREV, skin.prev_button,
            layout::RIGHT_PREV_POS, layout::PAGER_SIZE);
        draw_action(BT_RIGHT_NEXT, skin.next_button,
            layout::RIGHT_NEXT_POS, layout::PAGER_SIZE);
    }

    void UICashShop::draw_action(uint16_t id,
        const cash_shop_skin::CashShopButtonSkin& source,
        Point<int16_t> pos, Point<int16_t> size) const
    {
        auto found = buttons.find(id);
        if (found == buttons.end())
            return;
        const size_t frame = found->second->is_active() ?
            button_frame(found->second->get_state()) : 3;
        const Texture& texture = source.frames[frame];
        texture.draw({ draw_position(pos + texture.get_origin()), size });
    }

    void UICashShop::draw_icon(const Texture& icon, Point<int16_t> center,
        int16_t max_side) const
    {
        if (!icon.is_valid())
            return;
        const Point<int16_t> dimensions = icon.get_dimensions();
        if (dimensions.x() <= 0 || dimensions.y() <= 0)
            return;
        Point<int16_t> size = dimensions;
        const int16_t longest = std::max(dimensions.x(), dimensions.y());
        if (max_side > 0 && longest > max_side)
        {
            size = {
                static_cast<int16_t>(
                    static_cast<int32_t>(dimensions.x()) * max_side / longest),
                static_cast<int16_t>(
                    static_cast<int32_t>(dimensions.y()) * max_side / longest)
            };
        }
        icon.draw(
            { draw_position(center - size / 2 + icon.get_origin()), size });
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
        position_buy_button();
    }

    void UICashShop::position_buy_button()
    {
        if (!selected_left ||
            *selected_left / view::CATALOG_PAGE_SIZE != left_page)
            return;
        buttons[BT_BUY]->set_position(
            layout::card_position(
                *selected_left % view::CATALOG_PAGE_SIZE) +
            layout::CARD_BUY_OFFSET);
    }

    UIElement::CursorResult UICashShop::send_cursor(
        bool pressed, Point<int16_t> cursorpos)
    {
        last_cursor_position = cursorpos;
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
        const bool over_catalog = last_cursor_position.x() <
            layout::SCROLL_SPLIT_X;
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
        case CashCurrency::MAPLE_POINT: return "地图点";
        case CashCurrency::NX_PREPAID: return "预付点";
        default: return "信用点";
        }
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
