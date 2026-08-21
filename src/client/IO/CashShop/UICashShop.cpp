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

#include "nlnx/nx.hpp"

#include <algorithm>
#include <stdexcept>

namespace jrc
{
    namespace view = cash_shop_view;

    UICashShop::UICashShop(std::shared_ptr<CashShopModel> value, bool is_female)
        : model(std::move(value)), female(is_female), right_pane(view::RightPane::LOCKER),
          currency(CashCurrency::NX_CREDIT), category(0), left_page(0), right_page(0),
          pending(false), pending_cash_id(0), last_cursor_position(),
          search(Text::A11M, Text::LEFT, Text::DARKGREY, {{48, 61}, {148, 79}}, 40),
          title(Text::A11B, Text::LEFT, Text::DARKGREY, "现金商城"),
          category_label(Text::A11M, Text::LEFT, Text::DARKGREY),
          currency_label(Text::A11M, Text::LEFT, Text::DARKGREY),
          search_label(Text::A11M, Text::LEFT, Text::DARKGREY, "搜索:"),
          balance_label(Text::A11M, Text::LEFT, Text::DARKGREY),
          right_mode_label(Text::A11B, Text::LEFT, Text::DARKGREY),
          transfer_label(Text::A11B, Text::CENTER, Text::DARKGREY, "", 64),
          left_page_label(Text::A11M, Text::CENTER, Text::DARKGREY, "", 100),
          right_page_label(Text::A11M, Text::CENTER, Text::DARKGREY, "", 100)
    {
        nl::node shop = nl::nx::ui["UIWindow2.img"]["Shop"];
        nl::node background = shop["backgrnd"];
        dimension = Texture(background).get_dimensions();
        if (dimension != Point<int16_t>{465, 328})
            throw std::runtime_error("Missing required classic Shop background");

        sprites.emplace_back(background);
        sprites.emplace_back(shop["backgrnd2"]);
        sprites.emplace_back(shop["backgrnd3"]);
        selection = shop["select"];

        buttons[BT_EXIT] = std::make_unique<MapleButton>(shop["BtExit"]);
        buttons[BT_BUY] = std::make_unique<MapleButton>(shop["BtBuy"]);
        buttons[BT_BUY_HIT] = std::make_unique<AreaButton>(
            Point<int16_t>{150, 42}, Point<int16_t>{80, 24});
        buttons[BT_SELECTED_TRANSFER] = std::make_unique<AreaButton>(
            Point<int16_t>{0, 0}, Point<int16_t>{200, 36});
        buttons[BT_TRANSFER] = std::make_unique<AreaButton>(
            Point<int16_t>{375, 38}, Point<int16_t>{80, 32});
        buttons[BT_CATEGORY] = std::make_unique<AreaButton>(
            Point<int16_t>{12, 22}, Point<int16_t>{136, 16});
        buttons[BT_CURRENCY] = std::make_unique<AreaButton>(
            Point<int16_t>{12, 40}, Point<int16_t>{136, 16});
        buttons[BT_RIGHT_MODE] = std::make_unique<AreaButton>(
            Point<int16_t>{242, 40}, Point<int16_t>{136, 20});
        buttons[BT_LEFT_PREVIOUS] = std::make_unique<AreaButton>(
            Point<int16_t>{12, 92}, Point<int16_t>{36, 18});
        buttons[BT_LEFT_NEXT] = std::make_unique<AreaButton>(
            Point<int16_t>{172, 92}, Point<int16_t>{36, 18});
        buttons[BT_RIGHT_PREVIOUS] = std::make_unique<AreaButton>(
            Point<int16_t>{242, 92}, Point<int16_t>{36, 18});
        buttons[BT_RIGHT_NEXT] = std::make_unique<AreaButton>(
            Point<int16_t>{406, 92}, Point<int16_t>{36, 18});

        for (size_t row = 0; row < view::ROWS_PER_PANE; ++row)
        {
            buttons[BT_LEFT_FIRST + row] = std::make_unique<AreaButton>(
                Point<int16_t>{8, static_cast<int16_t>(116 + row * 42)},
                Point<int16_t>{200, 36});
            buttons[BT_RIGHT_FIRST + row] = std::make_unique<AreaButton>(
                Point<int16_t>{242, static_cast<int16_t>(116 + row * 42)},
                Point<int16_t>{200, 36});
            left_names[row] = Text(Text::A11M, Text::LEFT, Text::DARKGREY, "", 152);
            left_details[row] = Text(Text::A11M, Text::LEFT, Text::DARKGREY, "", 152);
            right_names[row] = Text(Text::A11M, Text::LEFT, Text::DARKGREY, "", 152);
            right_details[row] = Text(Text::A11M, Text::LEFT, Text::DARKGREY, "", 152);
        }

        search.set_enter_callback([this](const std::string&) { rebuild_catalog(); });
        update_screen(Constants::viewwidth(), Constants::viewheight());
        rebuild_catalog();
    }

    void UICashShop::draw(float alpha) const
    {
        UIElement::draw(alpha);
        title.draw(position + Point<int16_t>{12, 5});
        category_label.draw(position + Point<int16_t>{12, 22});
        currency_label.draw(position + Point<int16_t>{12, 40});
        search_label.draw(position + Point<int16_t>{12, 62});
        search.draw(position);
        balance_label.draw(position + Point<int16_t>{242, 5});
        right_mode_label.draw(position + Point<int16_t>{242, 40});
        transfer_label.draw(position + Point<int16_t>{390, 47});
        left_page_label.draw(position + Point<int16_t>{60, 95});
        right_page_label.draw(position + Point<int16_t>{290, 95});

        for (size_t row = 0; row < view::ROWS_PER_PANE; ++row)
        {
            const size_t left_index = left_page * view::ROWS_PER_PANE + row;
            const size_t right_index = right_page * view::ROWS_PER_PANE + row;
            const Point<int16_t> left = position + row_position(false, row);
            const Point<int16_t> right = position + row_position(true, row);

            if (left_index < filtered.size())
            {
                if (selected_left && *selected_left == left_index)
                    selection.draw(left + Point<int16_t>{35, -1});
                const int32_t item_id = model->catalog()[filtered[left_index]].item_id;
                if (const ItemData& data = ItemData::get(item_id))
                    data.get_icon(false).draw(DrawArgument(left + Point<int16_t>{0, 32}));
                left_names[row].draw(left + Point<int16_t>{40, -1});
                left_details[row].draw(left + Point<int16_t>{40, 17});
            }
            if (right_index < right_count())
            {
                if (selected_right && *selected_right == right_index)
                    selection.draw(right + Point<int16_t>{35, -1});
                if (const ItemData& data = ItemData::get(right_item_id(right_index)))
                    data.get_icon(false).draw(DrawArgument(right + Point<int16_t>{0, 32}));
                right_names[row].draw(right + Point<int16_t>{40, -1});
                right_details[row].draw(right + Point<int16_t>{40, 17});
            }
        }
    }

    void UICashShop::update()
    {
        UIElement::update();
        search.update(position);
    }

    void UICashShop::update_screen(int16_t width, int16_t height)
    {
        position = { static_cast<int16_t>((width - dimension.x()) / 2),
            static_cast<int16_t>((height - dimension.y()) / 2) };
    }

    UIElement::CursorResult UICashShop::send_cursor(bool pressed, Point<int16_t> cursorpos)
    {
        last_cursor_position = cursorpos - position;
        const Rectangle<int16_t> search_hitbox(
            position + Point<int16_t>{12, 55}, position + Point<int16_t>{208, 86});
        if (search_hitbox.contains(cursorpos))
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
        size_t& page = last_cursor_position.x() < 232 ? left_page : right_page;
        const size_t count = last_cursor_position.x() < 232 ? filtered.size() : right_count();
        if (yoffset > 0 && page > 0)
            --page;
        else if (yoffset < 0 && page + 1 < view::page_count(count))
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
        else if (id == BT_CATEGORY)
        {
            category = view::next_category(category);
            rebuild_catalog();
        }
        else if (id == BT_CURRENCY)
        {
            currency = view::next_currency(currency);
            refresh();
        }
        else if (id == BT_RIGHT_MODE)
        {
            change_right_pane(view::toggle_pane(right_pane));
        }
        else if (id == BT_LEFT_PREVIOUS && left_page > 0)
            --left_page;
        else if (id == BT_LEFT_NEXT && left_page + 1 < view::page_count(filtered.size()))
            ++left_page;
        else if (id == BT_RIGHT_PREVIOUS && right_page > 0)
            --right_page;
        else if (id == BT_RIGHT_NEXT && right_page + 1 < view::page_count(right_count()))
            ++right_page;
        else if ((id == BT_BUY || id == BT_BUY_HIT) && selected_left)
            request_purchase(*selected_left);
        else if (id == BT_TRANSFER && selected_right)
            request_transfer(*selected_right);
        else if (id == BT_SELECTED_TRANSFER && selected_right)
            request_transfer(*selected_right);
        else if (id >= BT_LEFT_FIRST && id < BT_RIGHT_FIRST)
        {
            const size_t index = left_page * view::ROWS_PER_PANE + id - BT_LEFT_FIRST;
            if (index < filtered.size())
            {
                if (selected_left && *selected_left == index)
                    request_purchase(index);
                else
                    selected_left = index;
            }
        }
        else if (id >= BT_RIGHT_FIRST && id < BT_RIGHT_FIRST + view::ROWS_PER_PANE)
        {
            const size_t index = right_page * view::ROWS_PER_PANE + id - BT_RIGHT_FIRST;
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
