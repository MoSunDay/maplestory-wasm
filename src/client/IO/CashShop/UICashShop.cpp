#include "UICashShop.h"
#include "CashShopDraw.h"

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

    using cash_shop_draw::draw_position;
    using cash_shop_draw::draw_size;
    using cash_shop_draw::fill_rect;
    using cash_shop_draw::outline_rect;
    using cash_shop_draw::currency_label_position;

    UICashShop::UICashShop(std::shared_ptr<CashShopModel> value, bool is_female)
        : model(std::move(value)), female(is_female),
          right_pane(view::RightPane::LOCKER),
          currency(CashCurrency::NX_CREDIT), category(0),
          equip_sub(0), equip_menu_open(false), left_page(0),
          right_page(0), pending_kind_(cash_shop_settlement::PendingKind::NONE),
          balances_loaded_(false), balance_failed_(false),
          require_reenter_(false), deferred_balance_error_(false),
          balance_deadline_(), pending_deadline_(), preview_dirty(true),
          pending_cash_id(0),
          last_cursor_position(),
          skin(cash_shop_skin::load_cash_shop_skin()),
          search(Text::A11M, Text::LEFT, Text::WHITE,
              layout::SEARCH_FIELD_BOUNDS, 40),
          title(Text::A12B, Text::LEFT, Text::WHITE, "现金商城"),
          category_title(Text::A11B, Text::LEFT, Text::BLACK, "商品分类"),
          equip_menu_title(Text::A11B, Text::LEFT, Text::BLACK, "装备分类"),
          equip_close_label(Text::A11M, Text::RIGHT, Text::DARKGREY, "← 返回"),
          pane_title(Text::A12B, Text::LEFT, Text::WHITE),
          pane_info(Text::A11M, Text::RIGHT, Text::LIGHTGREY),
          search_label(Text::A11M, Text::LEFT, Text::LIGHTGREY, "搜索:"),
          search_clear_label(Text::A11M, Text::CENTER, Text::BLACK, "×"),
          transfer_hint(Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
              layout::TRANSFER_HINT_WIDTH),
          details_hint(Text::A11M, Text::CENTER, Text::LIGHTGREY,
              "选择物品查看详情", layout::DETAILS_HINT_WIDTH),
          details_name(Text::A11B, Text::CENTER, Text::WHITE, "",
              layout::DETAILS_TEXT_WIDTH),
          details_count(Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
              layout::DETAILS_TEXT_WIDTH),
          buy_label(Text::A11M, Text::CENTER, Text::BLACK, "购买"),
          reset_label(Text::A11M, Text::CENTER, Text::BLACK, "重置"),
          transfer_label(Text::A11M, Text::CENTER, Text::WHITE, "存入"),
          charge_label(Text::A11M, Text::CENTER, Text::BLACK, "充值"),
          exit_label(Text::A11M, Text::CENTER, Text::WHITE, "关闭")
    {
        // Hit areas are created (and later re-anchored on resize) from the
        // shared scaled-geometry table.
        cash_shop_buttons::apply_geometry(buttons);

        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
        {
            card_names[card] = Text(
                Text::A11M, Text::CENTER, Text::BLACK, "",
                layout::CARD_NAME_WIDTH);
            card_meta[card] = Text(
                Text::A11M, Text::CENTER, Text::DARKGREY, "",
                layout::CARD_META_WIDTH);
        }
        for (size_t slot = 0; slot < view::RIGHT_PAGE_SIZE; ++slot)
        {
            slot_counts[slot] = Text(Text::A11M, Text::RIGHT, Text::BLACK);
        }
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
        {
            category_labels[entry] = Text(
                Text::A11M, Text::LEFT, Text::BLACK,
                view::category_name(entry), layout::CATEGORY_LABEL_WIDTH);
            category_counts[entry] = Text(
                Text::A11M, Text::RIGHT, Text::DARKGREY, "",
                layout::CATEGORY_COUNT_WIDTH);
        }
        for (int8_t sub = 0; sub < view::EQSUB_COUNT; ++sub)
        {
            equip_sub_labels[sub] = Text(
                Text::A11M, Text::LEFT, Text::DARKGREY,
                view::equip_sub_name(sub), layout::EQSUB_LABEL_WIDTH);
            equip_sub_counts[sub] = Text(
                Text::A11M, Text::RIGHT, Text::DARKGREY, "",
                layout::EQSUB_COUNT_WIDTH);
        }
        for (size_t choice = 0; choice < CURRENCY_CHOICES; ++choice)
        {
            balance_labels[choice] = Text(
                Text::A11M, Text::LEFT, Text::LIGHTGREY,
                currency_short_name(currency_choice(choice)));
            currency_labels[choice] = Text(
                Text::A11M, Text::CENTER, Text::LIGHTGREY,
                currency_short_name(currency_choice(choice)),
                layout::CURRENCY_SIZE.x());
        }
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
        catalog_info = Text(
            Text::A11M, Text::RIGHT, Text::LIGHTGREY, "",
            layout::CATALOG_INFO_WIDTH);
        catalog_hint = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY,
            "点击卡片选中 · 双击/回车购买", layout::CATALOG_HINT_WIDTH);
        legend_title = Text(
            Text::A11B, Text::LEFT, Text::BLACK, "徽章说明");
        legend_note = Text(
            Text::A11M, Text::LEFT, Text::DARKGREY,
            "徽章随商品属性\n自动显示", layout::LEGEND_NOTE_WIDTH);
        rail_help = Text(
            Text::A11M, Text::LEFT, Text::DARKGREY,
            "Enter 搜索\n单击选中\n双击购买", layout::RAIL_HELP_WIDTH);
        rail_status = Text(
            Text::A11M, Text::LEFT, Text::DARKGREY, "",
            layout::EQSUB_STATUS_WIDTH);
        details_title = Text(
            Text::A11B, Text::LEFT, Text::WHITE, "物品详情");
        empty_catalog_text = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::EMPTY_CATALOG_LABEL_WIDTH);
        details_meta = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::DETAILS_TEXT_WIDTH);
        details_extra = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::DETAILS_TEXT_WIDTH);
        featured_title = Text(
            Text::A11B, Text::LEFT, Text::WHITE, "本期推荐");
        featured_name = Text(
            Text::A11B, Text::LEFT, Text::WHITE, "",
            layout::FEATURED_TEXT_WIDTH);
        featured_meta = Text(
            Text::A11M, Text::LEFT, Text::LIGHTGREY, "",
            layout::FEATURED_TEXT_WIDTH);
        featured_hint = Text(
            Text::A11M, Text::CENTER, Text::LIGHTGREY, "",
            layout::FEATURED_HINT_WIDTH);
        constexpr view::CommodityBadge legend_badges[5] = {
            view::CommodityBadge::SALE, view::CommodityBadge::LIMITED,
            view::CommodityBadge::EVENT, view::CommodityBadge::NEW,
            view::CommodityBadge::HOT
        };
        for (size_t badge = 0; badge < 5; ++badge)
            legend_labels[badge] = Text(
                Text::A11M, Text::LEFT, Text::DARKGREY,
                view::badge_name(legend_badges[badge]),
                layout::LEGEND_LABEL_WIDTH);

        search.set_enter_callback([this](const std::string& text)
        {
            if (text == applied_query)
                return;
            applied_query = text;
            rebuild_catalog();
        });
        update_screen(Constants::viewwidth(), Constants::viewheight());
        preview_look = Stage::get().get_player().get_look();
        rebuild_catalog();
        refresh_category_icons();
    }

    void UICashShop::draw(float alpha) const
    {
        (void)alpha;
        // The backdrop fills the current viewport, independent of the 800x600
        // content origin; the shader shifts it back by VIEWYOFFSET.
        skin.background.draw({ { 0, -Constants::VIEWYOFFSET }, { Constants::viewwidth(), Constants::viewheight() } });

        const auto state_of = [this](uint16_t id) -> Button::State
        {
            const auto found = buttons.find(id);
            return found != buttons.end() ? found->second->get_state() :
                Button::NORMAL;
        };
        // Dark header band over the painted artwork. It spans the full
        // viewport width so the gutters left of the scaled canvas are
        // covered too.
        const int16_t band_top = draw_position(layout::TOP_BAR_POS).y();
        const int16_t band_height =
            layout::scale_coord(layout::TOP_BAR_SIZE.y());
        GraphicsGL::get().drawrectangle(0, band_top,
            Constants::viewwidth(), band_height,
            0.04f, 0.05f, 0.07f, 0.92f);
        GraphicsGL::get().drawrectangle(0,
            static_cast<int16_t>(band_top + band_height - 1),
            Constants::viewwidth(), 1, 0.88f, 0.52f, 0.16f, 0.35f);

        fill_rect({ 6, 8 }, { 3, 16 }, 0.88f, 0.52f, 0.16f, 1.0f);
        title.draw(draw_position(layout::TITLE_POS));
        draw_balance_icons();

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
        // Design-space-scaled Textfield bounds move with the content origin.
        search.draw(layout::origin());
        if (!applied_query.empty())
        {
            const auto clear = buttons.find(BT_SEARCH_CLEAR);
            const bool clear_active = clear != buttons.end() &&
                clear->second->is_active();
            const bool clear_hovered = clear_active &&
                clear->second->get_state() == Button::MOUSEOVER;
            fill_rect(layout::SEARCH_CLEAR_POS, layout::SEARCH_CLEAR_SIZE,
                0.88f, 0.52f, 0.16f, clear_hovered ? 1.0f : 0.90f);
            outline_rect(layout::SEARCH_CLEAR_POS, layout::SEARCH_CLEAR_SIZE,
                0.98f, 0.72f, 0.36f, 1.0f);
            search_clear_label.draw(draw_position(layout::pill_label(
                layout::SEARCH_CLEAR_POS, layout::SEARCH_CLEAR_SIZE)));
        }

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
        draw_catalog_info();

        // One continuous light rail. The equipment sub-menu swaps its
        // contents in place, so no popup ever covers a catalog column.
        fill_rect(layout::RAIL_PANEL_POS, layout::RAIL_PANEL_SIZE,
            0.97f, 0.98f, 1.0f, 0.30f);
        outline_rect(layout::RAIL_PANEL_POS, layout::RAIL_PANEL_SIZE,
            0.0f, 0.0f, 0.0f, 0.14f);
        fill_rect(layout::RAIL_PANEL_POS,
            { layout::RAIL_PANEL_SIZE.x(), 3 },
            0.88f, 0.52f, 0.16f, 0.90f);
        if (category == view::EQUIP_CATEGORY && equip_menu_open)
        {
            draw_equip_menu();
        }
        else
        {
            category_title.draw(draw_position(layout::CATEGORY_TITLE_POS));
            for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
            {
                const Point<int16_t> base = layout::category_position(entry);
                const bool selected = entry == category;
                const bool hovered = !selected &&
                    state_of(BT_TAB_FIRST + entry) == Button::MOUSEOVER;
                if (selected)
                {
                    fill_rect(base, layout::CATEGORY_SIZE,
                        0.88f, 0.52f, 0.16f, 0.96f);
                    fill_rect(base + layout::CATEGORY_LINE_OFFSET,
                        layout::CATEGORY_LINE_SIZE,
                        0.55f, 0.30f, 0.08f, 1.0f);
                }
                else if (hovered)
                {
                    fill_rect(base, layout::CATEGORY_SIZE,
                        1.0f, 1.0f, 1.0f, 0.48f);
                }
                const Point<int16_t> icon_center =
                    base + layout::CATEGORY_ICON_CENTER;
                fill_rect(
                    { static_cast<int16_t>(icon_center.x() - 8),
                        static_cast<int16_t>(icon_center.y() - 8) },
                    { 16, 16 },
                    selected ? 0.30f : 0.08f,
                    selected ? 0.20f : 0.07f,
                    selected ? 0.08f : 0.05f, 0.55f);
                if (entry == 0)
                {
                    const float cell = selected ? 1.0f : 0.25f;
                    for (int8_t square = 0; square < 4; ++square)
                        fill_rect(
                            { static_cast<int16_t>(icon_center.x() - 6 +
                                (square % 2) * 7),
                              static_cast<int16_t>(icon_center.y() - 6 +
                                (square / 2) * 7) },
                            { 5, 5 }, cell, cell, cell, 0.95f);
                }
                else if (category_icon_ids[entry] != 0)
                {
                    if (const ItemData& data =
                            ItemData::get(category_icon_ids[entry]))
                        draw_icon(data.get_icon(false), icon_center,
                            layout::CATEGORY_ICON_MAX);
                }
                category_labels[entry].draw(
                    draw_position(base + layout::CATEGORY_LABEL_OFFSET));
                if (!selected)
                    fill_rect(
                        { static_cast<int16_t>(base.x() + 8),
                          static_cast<int16_t>(base.y() +
                            layout::CATEGORY_SIZE.y() - 1) },
                        { static_cast<int16_t>(
                            layout::CATEGORY_SIZE.x() - 16), 1 },
                        0.0f, 0.0f, 0.0f, 0.06f);
            }
            draw_category_counts();
            draw_sidebar_legend();
        }

        const auto left_range =
            view::page_range(left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        if (left_range.first == left_range.second)
        {
            skin.no_item.draw({ draw_position(layout::EMPTY_CATALOG_CENTER -
                layout::EMPTY_HINT_SIZE / 2),
                draw_size(layout::EMPTY_HINT_SIZE) });
            empty_catalog_text.draw(
                draw_position(layout::EMPTY_CATALOG_LABEL_POS));
        }
        else
        {
            for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
            {
                const size_t index = left_range.first + card;
                const Point<int16_t> base = layout::card_position(card);
                skin.card.draw(
                    { draw_position(base), draw_size(layout::CARD_SIZE) });
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
        draw_card_meta();
        page_labels[0].draw(draw_position(layout::LEFT_PAGE_POS));
        draw_catalog_hint();

        draw_right_pane();

        // Selected item details.
        fill_rect(layout::DETAILS_PANEL_POS, layout::DETAILS_PANEL_SIZE,
            0.06f, 0.07f, 0.09f, 0.90f);
        outline_rect(layout::DETAILS_PANEL_POS, layout::DETAILS_PANEL_SIZE,
            0.25f, 0.28f, 0.34f, 1.0f);
        fill_rect(layout::DETAILS_PANEL_POS, { 64, 2 },
            0.88f, 0.52f, 0.16f, 1.0f);
        fill_rect(layout::DETAILS_PANEL_POS,
            { layout::DETAILS_PANEL_SIZE.x(), 22 },
            0.04f, 0.05f, 0.07f, 0.94f);
        fill_rect(layout::DETAILS_HEADER_LINE_POS,
            layout::DETAILS_HEADER_LINE_SIZE, 0.25f, 0.28f, 0.34f, 1.0f);
        details_title.draw(draw_position(layout::DETAILS_TITLE_POS));
        const bool details_visible = selected_right &&
            *selected_right / view::RIGHT_PAGE_SIZE == right_page &&
            *selected_right < right_count();
        if (details_visible)
        {
            if (const ItemData& data = ItemData::get(
                    right_item_id(*selected_right)))
                draw_icon(data.get_icon(false), layout::DETAILS_ICON_CENTER,
                    layout::DETAILS_ICON_MAX);
        }
        draw_details_text();
        draw_feature_panel();

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

    void UICashShop::update()
    {
        UIElement::update();
        // Queued errors are shown here, never from the callback that raised
        // them: emplace<UIOk> resets the NOTICE slot, which would destroy a
        // UIYesNo that is still executing its yes-handler.
        if (!deferred_error.empty())
        {
            const std::string message = std::move(deferred_error);
            deferred_error.clear();
            UI::get().emplace<UIOk>(message, []() {});
        }
        search.update(layout::origin());
        if (preview_dirty)
        {
            preview_dirty = false;
            refresh_preview();
        }
        const auto now = cash_shop_settlement::Clock::now();
        // showCash pushes the balances exactly once, so arm the deadline the
        // first time update() runs. A lost push must not lock the shop
        // forever; it only shows a retry hint and keeps browsing alive.
        if (!balances_loaded_ && !balance_failed_ && !balance_deadline_)
            balance_deadline_ = now +
                cash_shop_settlement::BALANCE_TIMEOUT +
                cash_shop_settlement::SETTLE_GRACE;
        if (balance_deadline_ && cash_shop_settlement::deadline_reached(
                now, *balance_deadline_))
        {
            balance_deadline_.reset();
            balance_failed_ = true;
            deferred_balance_error_ = true;
            deferred_error = "商城余额加载超时，请重试。";
            refresh();
        }
        if (pending_deadline_ && cash_shop_settlement::deadline_reached(
                now, *pending_deadline_))
        {
            const bool exiting =
                pending_kind_ == cash_shop_settlement::PendingKind::EXIT;
            clear_pending();
            require_reenter_ = true;
            // The exit path disabled the UI; a timed-out exit must hand it
            // back or the frozen screen could never be left.
            if (exiting)
                UI::get().enable();
            deferred_balance_error_ = false;
            deferred_error = exiting ? "退出商城超时，请重试。" :
                "商城操作超时，请重试。";
            refresh();
        }
    }

    void UICashShop::update_screen(int16_t, int16_t)
    {
        position = layout::origin();
        dimension = layout::scale_point(
            { layout::VIEW_WIDTH, layout::VIEW_HEIGHT });
        cash_shop_buttons::apply_geometry(buttons);
        position_buy_button();
        // The field bounds stay in unshifted design-canvas space (scaled but
        // without the origin) because search.draw/search.update receive
        // layout::origin() as their parent position.
        search.set_bounds({ layout::scale_point(
            layout::SEARCH_FIELD_BOUNDS.getlt()), layout::scale_point(
            layout::SEARCH_FIELD_BOUNDS.getrb()) });
    }

    void UICashShop::position_buy_button()
    {
        if (!left_selection_visible())
            return;
        buttons[BT_BUY]->set_position(
            layout::scale_point(
                layout::card_position(
                    *selected_left % view::CATALOG_PAGE_SIZE) +
                layout::CARD_BUY_OFFSET));
    }

    UIElement::CursorResult UICashShop::send_cursor(
        bool pressed, Point<int16_t> cursorpos)
    {
        last_cursor_position = cursorpos;
        if (layout::search_hitbox().contains(last_cursor_position))
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
        const bool over_catalog =
            last_cursor_position.x() < layout::scroll_split_x();
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
        if (pressed && escape && can_exit())
            button_pressed(BT_EXIT);
        else if (pressed && keycode == KeyAction::RETURN && can_mutate() &&
            left_selection_visible())
            request_purchase(*selected_left);
    }

    Button::State UICashShop::button_pressed(uint16_t id)
    {
        if (is_pending())
            return Button::DISABLED;
        if (id == BT_EXIT)
        {
            // Leaving is tracked as a pending operation so a lost SetField
            // reply is recoverable, but its reply is not an error.
            begin_pending(cash_shop_settlement::PendingKind::EXIT, 0);
            UI::get().disable();
            LeaveCashShopPacket().dispatch();
        }
        else if (id == BT_CHARGE)
        {
            UI::get().emplace<UIOk>("当前版本暂不支持充值。", []() {});
        }
        else if (id == BT_RESET) clear_tryon();
        else if (id == BT_SEARCH_CLEAR) clear_search();
        else if (id == BT_BUY && can_mutate() && left_selection_visible())
            request_purchase(*selected_left);
        else if (id == BT_TRANSFER && can_mutate() && selected_right)
            request_transfer(*selected_right);
        else if (id >= BT_TAB_FIRST && id < BT_TAB_LAST)
        {
            const int8_t entry = static_cast<int8_t>(id - BT_TAB_FIRST);
            if (entry == category && entry == view::EQUIP_CATEGORY)
            {
                equip_menu_open = !equip_menu_open;
                refresh();
            }
            else if (entry != category)
            {
                category = entry;
                equip_menu_open = entry == view::EQUIP_CATEGORY;
                if (entry != view::EQUIP_CATEGORY)
                    equip_sub = 0;
                rebuild_catalog();
            }
        }
        else if (id == BT_EQSUB_CLOSE || (id >= BT_EQSUB_FIRST &&
            id < BT_EQSUB_LAST))
            press_equip_menu(id);
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
        else if (id == BT_FEATURED)
        {
            if (const auto highlighted = highlighted_index())
                selected_left = *highlighted;
        }
        else if (id >= BT_CARD_FIRST && id < BT_CARD_LAST)
        {
            const size_t index = left_page * view::CATALOG_PAGE_SIZE +
                (id - BT_CARD_FIRST);
            if (index < filtered.size())
            {
                if (selected_left && *selected_left == index)
                    request_purchase(index);
                else
                {
                    selected_left = index;
                    add_tryon(index);
                }
            }
        }
        else if (id >= BT_SLOT_FIRST && id < BT_SLOT_LAST)
        {
            const size_t index = right_page * view::RIGHT_PAGE_SIZE +
                (id - BT_SLOT_FIRST);
            if (index < right_count())
                selected_right = index;
        }
        refresh();
        return Button::NORMAL;
    }
}
