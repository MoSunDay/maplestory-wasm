#include "UICashShop.h"
#include "CashShopDraw.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"

#include <algorithm>

namespace jrc
{
    namespace view = cash_shop_view;
    namespace layout = cash_shop_layout;

    using cash_shop_draw::draw_position;
    using cash_shop_draw::draw_size;
    using cash_shop_draw::fill_rect;
    using cash_shop_draw::outline_rect;

    namespace
    {
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

        const Texture* badge_texture(
            const cash_shop_skin::CashShopSkin& skin,
            view::CommodityBadge badge)
        {
            switch (badge)
            {
            case view::CommodityBadge::SALE: return &skin.badge_sale;
            case view::CommodityBadge::LIMITED: return &skin.badge_limited;
            case view::CommodityBadge::EVENT: return &skin.badge_event;
            case view::CommodityBadge::NEW: return &skin.badge_new;
            case view::CommodityBadge::HOT: return &skin.badge_hot;
            case view::CommodityBadge::NONE: break;
            }
            return nullptr;
        }

        // Badge artwork is a 31x29 bitmap stretched onto the requested cell.
        void draw_badge(const cash_shop_skin::CashShopSkin& skin,
            view::CommodityBadge badge, Point<int16_t> position,
            Point<int16_t> size)
        {
            const Texture* texture = badge_texture(skin, badge);
            if (!texture)
                return;
            texture->draw({ draw_position(position), draw_size(size) });
        }
    }

    void UICashShop::draw_balance_icons() const
    {
        const Texture* icons[3] = {
            &skin.currency_icon[0], &skin.currency_icon[1], &skin.nx_icon
        };
        for (size_t row = 0; row < 3; ++row)
            icons[row]->draw(draw_position(
                { layout::BALANCE_ICON_XS[row], layout::BALANCE_ICON_Y }));
    }

    void UICashShop::draw_catalog_info() const
    {
        fill_rect(layout::CATALOG_INFO_BOX_POS,
            layout::CATALOG_INFO_BOX_SIZE, 0.06f, 0.07f, 0.09f, 0.88f);
        const bool filtered = !applied_query.empty();
        outline_rect(layout::CATALOG_INFO_BOX_POS,
            layout::CATALOG_INFO_BOX_SIZE,
            filtered ? 0.88f : 0.25f, filtered ? 0.52f : 0.28f,
            filtered ? 0.16f : 0.34f, 1.0f);
        catalog_info.draw(draw_position(layout::CATALOG_INFO_POS));
    }

    void UICashShop::draw_category_counts() const
    {
        for (int8_t entry = 0; entry < view::CATEGORY_COUNT; ++entry)
            category_counts[entry].draw(draw_position(
                layout::category_position(entry) +
                layout::CATEGORY_COUNT_OFFSET));
    }

    void UICashShop::draw_card_meta() const
    {
        const auto left_range = view::page_range(
            left_page, filtered.size(), view::CATALOG_PAGE_SIZE);
        for (size_t card = 0; card < view::CATALOG_PAGE_SIZE; ++card)
        {
            const size_t index = left_range.first + card;
            if (index >= left_range.second)
                continue;
            const Point<int16_t> base = layout::card_position(card);
            card_meta[card].draw(
                draw_position(base + layout::CARD_META_POS));
            const CashCommodity& item = model->catalog()[filtered[index]];
            draw_badge(skin, view::commodity_badge(item),
                base + layout::CARD_BADGE_POS, layout::CARD_BADGE_SIZE);
        }
    }

    void UICashShop::draw_details_text() const
    {
        const bool visible = selected_right &&
            *selected_right / view::RIGHT_PAGE_SIZE == right_page &&
            *selected_right < right_count();
        if (!visible)
        {
            details_hint.draw(draw_position(layout::DETAILS_HINT_CENTER));
            return;
        }
        details_name.draw(draw_position(layout::DETAILS_TEXT_CENTER));
        details_count.draw(draw_position(
            { layout::DETAILS_TEXT_CENTER.x(), layout::DETAILS_COUNT_Y }));
        details_meta.draw(draw_position(
            { layout::DETAILS_TEXT_CENTER.x(), layout::DETAILS_META_Y }));
        details_extra.draw(draw_position(
            { layout::DETAILS_TEXT_CENTER.x(), layout::DETAILS_EXTRA_Y }));
    }

    void UICashShop::draw_feature_panel() const
    {
        const Point<int16_t> panel = layout::FEATURED_PANEL_POS;
        const Point<int16_t> panel_size = layout::FEATURED_PANEL_SIZE;
        skin.preview[left_page % 3].draw(
            { draw_position(panel), draw_size(panel_size) });
        fill_rect(panel, panel_size, 0.04f, 0.05f, 0.07f, 0.82f);
        fill_rect(panel, layout::FEATURED_HEADER_SIZE,
            0.02f, 0.03f, 0.05f, 0.94f);
        fill_rect(panel, { 64, 2 }, 0.88f, 0.52f, 0.16f, 1.0f);
        fill_rect(layout::FEATURED_HEADER_LINE_POS,
            layout::FEATURED_HEADER_LINE_SIZE, 0.88f, 0.52f, 0.16f, 0.72f);
        outline_rect(panel, panel_size, 0.25f, 0.28f, 0.34f, 1.0f);

        const auto featured = buttons.find(BT_FEATURED);
        if (featured != buttons.end() && featured->second->is_active() &&
            featured->second->get_state() == Button::MOUSEOVER)
            outline_rect(panel, panel_size, 1.0f, 0.62f, 0.22f, 1.0f, 2);
        featured_title.draw(draw_position(layout::FEATURED_TITLE_POS));

        const auto draw_hint = [this, &panel]()
        {
            fill_rect(layout::FEATURED_HINT_BAND_POS,
                layout::FEATURED_HINT_BAND_SIZE, 0.04f, 0.05f, 0.07f, 0.90f);
            fill_rect(layout::FEATURED_HINT_BAND_POS,
                { layout::FEATURED_HINT_BAND_SIZE.x(), 1 },
                0.88f, 0.52f, 0.16f, 0.65f);
            if (!tryon_items.empty())
                fill_rect({ panel.x(), layout::FEATURED_HINT_BAND_POS.y() },
                    { 3, layout::FEATURED_HINT_BAND_SIZE.y() },
                    0.88f, 0.52f, 0.16f, 0.85f);
            featured_hint.draw(draw_position(layout::FEATURED_HINT_POS));
        };
        const auto draw_reset = [this]()
        {
            const auto reset = buttons.find(BT_RESET);
            if (reset == buttons.end() || !reset->second->is_active())
                return;
            const Button::State state = reset->second->get_state();
            const float shade = state == Button::PRESSED ? 0.82f :
                state == Button::MOUSEOVER ? 1.16f : 1.0f;
            fill_rect(layout::RESET_POS, layout::RESET_SIZE,
                std::min(1.0f, 0.86f * shade),
                std::min(1.0f, 0.36f * shade),
                std::min(1.0f, 0.30f * shade), 0.98f);
            outline_rect(layout::RESET_POS, layout::RESET_SIZE,
                0.98f, 0.52f, 0.44f, 1.0f);
            reset_label.draw(draw_position(
                layout::pill_label(layout::RESET_POS, layout::RESET_SIZE)));
        };

        const auto highlighted = highlighted_index();
        if (!highlighted)
        {
            draw_hint();
            draw_reset();
            return;
        }

        const CashCommodity& item = model->catalog()[filtered[*highlighted]];
        draw_badge(skin, view::commodity_badge(item),
            layout::FEATURED_BADGE_POS, layout::FEATURED_BADGE_SIZE);

        const bool avatar_ready =
            (view::is_previewable_equip(item.item_id) ||
                !tryon_items.empty()) && avatar_preview_ready();
        if (avatar_ready)
        {
            fill_rect(layout::FEATURED_AVATAR_FRAME_POS,
                layout::FEATURED_AVATAR_FRAME_SIZE, 0.03f, 0.04f, 0.06f, 0.72f);
            outline_rect(layout::FEATURED_AVATAR_FRAME_POS,
                layout::FEATURED_AVATAR_FRAME_SIZE, 0.25f, 0.28f, 0.34f, 1.0f);
            fill_rect(layout::FEATURED_AVATAR_FLOOR_POS,
                layout::FEATURED_AVATAR_FLOOR_SIZE, 0.25f, 0.28f, 0.34f, 0.55f);
            preview_look.draw(draw_position(layout::FEATURED_AVATAR_POS),
                false, Stance::STAND1, Expression::DEFAULT);
        }
        else
        {
            fill_rect(layout::FEATURED_ICON_FRAME_POS,
                layout::FEATURED_ICON_FRAME_SIZE, 0.03f, 0.04f, 0.06f, 0.92f);
            outline_rect(layout::FEATURED_ICON_FRAME_POS,
                layout::FEATURED_ICON_FRAME_SIZE, 0.25f, 0.28f, 0.34f, 1.0f);
            if (const ItemData& data = ItemData::get(item.item_id))
                draw_icon(data.get_icon(false), layout::FEATURED_ICON_CENTER,
                    layout::FEATURED_ICON_MAX);
        }

        fill_rect(layout::FEATURED_TEXT_BLOCK_POS,
            layout::FEATURED_TEXT_BLOCK_SIZE, 0.03f, 0.04f, 0.06f, 0.90f);
        outline_rect(layout::FEATURED_TEXT_BLOCK_POS,
            layout::FEATURED_TEXT_BLOCK_SIZE, 0.18f, 0.21f, 0.26f, 0.85f);
        fill_rect(layout::FEATURED_TEXT_BLOCK_POS,
            { 3, layout::FEATURED_TEXT_BLOCK_SIZE.y() },
            0.88f, 0.52f, 0.16f, 0.70f);
        featured_name.draw(draw_position(layout::FEATURED_NAME_POS));

        const Point<int16_t> price = layout::FEATURED_PRICE_ICON_POS;
        skin.nx_icon.draw(draw_position(price));
        cash_shop_skin::draw_amount(skin,
            draw_position(price + Point<int16_t>{
                static_cast<int16_t>(skin.nx_icon.width() + 2), 0 }),
            item.price, Color::WHITE);
        featured_meta.draw(draw_position(layout::FEATURED_META_POS));

        draw_hint();
        draw_reset();
    }

    void UICashShop::draw_sidebar_legend() const
    {
        fill_rect(layout::CATEGORY_SECTION_LINE_POS,
            layout::CATEGORY_SECTION_LINE_SIZE, 0.0f, 0.0f, 0.0f, 0.14f);
        legend_title.draw(draw_position(layout::LEGEND_TITLE_POS));

        constexpr view::CommodityBadge badges[5] = {
            view::CommodityBadge::SALE, view::CommodityBadge::LIMITED,
            view::CommodityBadge::EVENT, view::CommodityBadge::NEW,
            view::CommodityBadge::HOT
        };
        for (size_t row = 0; row < 5; ++row)
        {
            const Point<int16_t> position = layout::LEGEND_ROW_POS +
                Point<int16_t>{ 0, static_cast<int16_t>(
                    row * layout::LEGEND_ROW_STEP) };
            draw_badge(skin, badges[row], position,
                layout::LEGEND_BADGE_SIZE);
            legend_labels[row].draw(draw_position(
                position + layout::LEGEND_LABEL_OFFSET));
        }
        fill_rect(layout::LEGEND_NOTE_SEPARATOR_POS,
            layout::LEGEND_NOTE_SEPARATOR_SIZE, 0.0f, 0.0f, 0.0f, 0.14f);
        legend_note.draw(draw_position(layout::LEGEND_NOTE_POS));
        rail_help.draw(draw_position(layout::RAIL_HELP_POS));
    }

    void UICashShop::draw_catalog_hint() const
    {
        skin.line.draw({ draw_position(layout::CATALOG_LINE_POS),
            draw_size(layout::CATALOG_LINE_SIZE) });
        catalog_hint.draw(draw_position(layout::CATALOG_HINT_POS));
    }

    void UICashShop::draw_equip_menu() const
    {
        if (category != view::EQUIP_CATEGORY || !equip_menu_open)
            return;

        const auto state_of = [this](uint16_t id) -> Button::State
        {
            const auto found = buttons.find(id);
            return found != buttons.end() ? found->second->get_state() :
                Button::NORMAL;
        };

        // The rail itself is already painted by draw(); this menu swaps the
        // category list for a compact sub-menu without covering the catalog.
        const bool back_hovered =
            state_of(BT_EQSUB_CLOSE) == Button::MOUSEOVER;
        if (back_hovered)
            fill_rect(layout::EQSUB_CLOSE_POS, layout::EQSUB_CLOSE_SIZE,
                1.0f, 0.62f, 0.22f, 0.30f);
        equip_menu_title.draw(draw_position(layout::EQSUB_TITLE_POS));
        equip_close_label.draw(draw_position(layout::EQSUB_BACK_POS));

        for (int8_t sub = 0; sub < view::EQSUB_COUNT; ++sub)
        {
            const Point<int16_t> base = layout::equip_sub_position(sub);
            const bool selected = sub == equip_sub;
            const bool hovered = !selected &&
                state_of(static_cast<uint16_t>(
                    BT_EQSUB_FIRST + sub)) == Button::MOUSEOVER;
            if (selected)
            {
                fill_rect(base, layout::EQSUB_ENTRY_SIZE,
                    0.88f, 0.52f, 0.16f, 0.96f);
                fill_rect(base, { 3, layout::EQSUB_ENTRY_SIZE.y() },
                    0.55f, 0.30f, 0.08f, 1.0f);
            }
            else if (hovered)
            {
                fill_rect(base, layout::EQSUB_ENTRY_SIZE,
                    1.0f, 1.0f, 1.0f, 0.58f);
            }
            else
            {
                fill_rect(base, layout::EQSUB_ENTRY_SIZE,
                    0.97f, 0.98f, 1.0f, 0.16f);
            }
            equip_sub_labels[sub].draw(
                draw_position(base + layout::EQSUB_LABEL_OFFSET));
            equip_sub_counts[sub].draw(
                draw_position(base + layout::EQSUB_COUNT_OFFSET));
            if (!selected)
                fill_rect(
                    { static_cast<int16_t>(base.x() + 8),
                      static_cast<int16_t>(base.y() +
                        layout::EQSUB_ENTRY_SIZE.y() - 1) },
                    { static_cast<int16_t>(
                        layout::EQSUB_ENTRY_SIZE.x() - 16), 1 },
                    0.0f, 0.0f, 0.0f, 0.06f);
        }

        fill_rect(layout::EQSUB_SECTION_LINE_POS,
            layout::EQSUB_SECTION_LINE_SIZE, 0.0f, 0.0f, 0.0f, 0.14f);
        fill_rect(layout::EQSUB_STATUS_PANEL_POS,
            layout::EQSUB_STATUS_PANEL_SIZE, 0.97f, 0.98f, 1.0f, 0.55f);
        outline_rect(layout::EQSUB_STATUS_PANEL_POS,
            layout::EQSUB_STATUS_PANEL_SIZE, 0.0f, 0.0f, 0.0f, 0.12f);
        rail_status.draw(draw_position(layout::EQSUB_STATUS_POS));
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
        texture.draw(
            { draw_position(pos) + texture.get_origin(), draw_size(size) });
    }

    void UICashShop::draw_icon(const Texture& icon, Point<int16_t> center,
        int16_t max_side) const
    {
        if (!icon.is_valid())
            return;
        const Point<int16_t> dimensions = icon.get_dimensions();
        if (dimensions.x() <= 0 || dimensions.y() <= 0)
            return;
        // The native glyph scales with the canvas while the fit-to-max rule
        // keeps it inside the scaled cell.
        Point<int16_t> size = draw_size(dimensions);
        const int16_t limit = layout::scale_coord(max_side);
        const int16_t longest = std::max(size.x(), size.y());
        if (limit > 0 && longest > limit)
        {
            size = {
                static_cast<int16_t>(
                    static_cast<int32_t>(size.x()) * limit / longest),
                static_cast<int16_t>(
                    static_cast<int32_t>(size.y()) * limit / longest)
            };
        }
        icon.draw(
            { draw_position(center) - size / 2 + icon.get_origin(), size });
    }

}
