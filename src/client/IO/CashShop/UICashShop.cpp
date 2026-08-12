#include "UICashShop.h"

#include "../UI.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../UITypes/UINotice.h"
#include "../../Character/Inventory/Inventory.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/CashShopPackets.h"
#include "../../Constants.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        const char* currency_name(CashCurrency currency)
        {
            switch (currency)
            {
            case CashCurrency::NX_CREDIT: return "NX Credit";
            case CashCurrency::MAPLE_POINT: return "Maple Point";
            case CashCurrency::NX_PREPAID: return "Prepaid NX";
            }
            return "NX";
        }

        std::string format_amount(int32_t amount)
        {
            std::string text = std::to_string(amount);
            for (int32_t index = static_cast<int32_t>(text.size()) - 3; index > 0; index -= 3)
                text.insert(static_cast<size_t>(index), ",");
            return text;
        }
    }

    UICashShop::UICashShop(std::shared_ptr<CashShopModel> value, bool is_female)
        : model(std::move(value)), female(is_female), mode(Mode::CATALOG),
          currency(CashCurrency::NX_CREDIT), category(0), page(0), pending(false),
          pending_cash_id(0),
          search(Text::A11M, Text::LEFT, Text::BLACK, {{250, 337}, {500, 357}}, 40),
          title(Text::A18M, Text::LEFT, Text::WHITE, "现金商城"),
          balance_label(Text::A11M, Text::LEFT, Text::WHITE),
          status_label(Text::A11B, Text::LEFT, Text::YELLOW)
    {
        nl::node cash_shop = nl::nx::ui["CashShop.img"];
        nl::node background = cash_shop["Base"]["backgrnd"];
        sprites.emplace_back(background);
        dimension = Texture(background).get_dimensions();
        if (dimension.x() <= 0 || dimension.y() <= 0)
            dimension = { 1024, 768 };

        buttons[BT_EXIT] = std::make_unique<MapleButton>(cash_shop["CSTab"]["BtExit"], 5, 728);
        buttons[BT_CREDIT] = std::make_unique<AreaButton>(Point<int16_t>{660, 6}, Point<int16_t>{105, 26});
        buttons[BT_POINTS] = std::make_unique<AreaButton>(Point<int16_t>{765, 6}, Point<int16_t>{105, 26});
        buttons[BT_PREPAID] = std::make_unique<AreaButton>(Point<int16_t>{870, 6}, Point<int16_t>{105, 26});
        buttons[BT_CATALOG] = std::make_unique<AreaButton>(Point<int16_t>{140, 300}, Point<int16_t>{95, 28});
        buttons[BT_LOCKER] = std::make_unique<AreaButton>(Point<int16_t>{240, 300}, Point<int16_t>{95, 28});
        buttons[BT_INVENTORY] = std::make_unique<AreaButton>(Point<int16_t>{340, 300}, Point<int16_t>{120, 28});

        for (uint16_t id = BT_CATEGORY_ALL; id <= BT_CATEGORY_PACKAGE; ++id)
            buttons[id] = std::make_unique<AreaButton>(
                Point<int16_t>{12, static_cast<int16_t>(92 + (id - BT_CATEGORY_ALL) * 34)},
                Point<int16_t>{112, 28});

        buttons[BT_PREVIOUS] = std::make_unique<AreaButton>(Point<int16_t>{760, 337}, Point<int16_t>{70, 26});
        buttons[BT_NEXT] = std::make_unique<AreaButton>(Point<int16_t>{840, 337}, Point<int16_t>{70, 26});

        nl::node buy = cash_shop["CSList"]["BtBuy"];
        for (size_t index = 0; index < ITEMS_PER_PAGE; ++index)
        {
            size_t column = index % 7;
            size_t row = index / 7;
            buttons[BT_ITEM_FIRST + index] = std::make_unique<MapleButton>(buy,
                Point<int16_t>{ static_cast<int16_t>(146 + 124 * column),
                    static_cast<int16_t>(523 + 205 * row) });
            item_names[index] = Text(Text::A11B, Text::CENTER, Text::BLACK, "", 96);
            item_prices[index] = Text(Text::A11M, Text::CENTER, Text::DARKGREY, "", 96);
        }

        search.set_enter_callback([this](const std::string&) { rebuild(); });
        update_screen(Constants::viewwidth(), Constants::viewheight());
        rebuild();
    }

    void UICashShop::draw(float alpha) const
    {
        UIElement::draw(alpha);
        title.draw(position + Point<int16_t>{18, 8});
        balance_label.draw(position + Point<int16_t>{470, 12});
        status_label.draw(position + Point<int16_t>{140, 342});
        search.draw(position);

        for (size_t index = 0; index < ITEMS_PER_PAGE; ++index)
        {
            if (page * ITEMS_PER_PAGE + index >= result_count())
                continue;
            size_t column = index % 7;
            size_t row = index / 7;
            Point<int16_t> base = position + Point<int16_t>{
                static_cast<int16_t>(192 + 124 * column),
                static_cast<int16_t>(470 + 205 * row) };
            int32_t item_id = 0;
            if (mode == Mode::CATALOG)
                item_id = model->catalog()[filtered[page * ITEMS_PER_PAGE + index]].item_id;
            else if (mode == Mode::LOCKER)
                item_id = model->locker()[page * ITEMS_PER_PAGE + index].item_id;
            else
                item_id = Stage::get().get_player().get_inventory().get_cash_items()[page * ITEMS_PER_PAGE + index].item_id;

            const ItemData& data = ItemData::get(item_id);
            if (data)
                data.get_icon(false).draw(DrawArgument(base));
            item_names[index].draw(base + Point<int16_t>{0, 42});
            item_prices[index].draw(base + Point<int16_t>{0, 60});
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
        if (search.get_bounds().contains(cursorpos))
        {
            Cursor::State state = search.send_cursor(cursorpos, pressed);
            if (pressed)
                UI::get().focus_textfield(&search);
            return { state, true };
        }
        return UIElement::send_cursor(pressed, cursorpos);
    }

    void UICashShop::send_scroll(double yoffset)
    {
        if (yoffset > 0 && page > 0)
            --page;
        else if (yoffset < 0 && (page + 1) * ITEMS_PER_PAGE < result_count())
            ++page;
        refresh_labels();
    }

    void UICashShop::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape && !pending)
            button_pressed(BT_EXIT);
    }

    void UICashShop::set_balances(CashBalances balances)
    {
        model->set_balances(balances);
        refresh_labels();
    }

    void UICashShop::replace_locker(std::vector<CashLockerItem> items)
    {
        model->replace_locker(std::move(items));
        rebuild();
    }

    void UICashShop::purchase_succeeded(CashLockerItem item)
    {
        model->add_locker(std::move(item));
        set_pending(false);
        rebuild();
    }

    void UICashShop::package_purchase_succeeded(std::vector<CashLockerItem> items)
    {
        for (auto& item : items)
            model->add_locker(std::move(item));
        set_pending(false);
        rebuild();
    }

    void UICashShop::take_succeeded()
    {
        model->remove_locker(pending_cash_id);
        set_pending(false);
        rebuild();
    }

    void UICashShop::put_succeeded(CashLockerItem item)
    {
        Stage::get().get_player().get_inventory().remove_cash_item(pending_cash_id);
        model->add_locker(std::move(item));
        set_pending(false);
        rebuild();
    }

    void UICashShop::show_error(const std::string& message)
    {
        set_pending(false);
        UI::get().emplace<UIOk>(message, []() {});
    }

    void UICashShop::reconnect_failed(const std::string& address)
    {
        UI::get().enable();
        show_error("返回游戏服务器失败：" + address);
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
            return Button::NORMAL;
        }
        if (id >= BT_CREDIT && id <= BT_PREPAID)
        {
            constexpr CashCurrency currencies[] = { CashCurrency::NX_CREDIT,
                CashCurrency::MAPLE_POINT, CashCurrency::NX_PREPAID };
            currency = currencies[id - BT_CREDIT];
            refresh_labels();
            return Button::NORMAL;
        }
        if (id >= BT_CATALOG && id <= BT_INVENTORY)
        {
            set_mode(static_cast<Mode>(id - BT_CATALOG));
            return Button::NORMAL;
        }
        if (id >= BT_CATEGORY_ALL && id <= BT_CATEGORY_PACKAGE)
        {
            set_category(static_cast<int8_t>(id - BT_CATEGORY_ALL));
            return Button::NORMAL;
        }
        if (id == BT_PREVIOUS && page > 0)
            --page;
        else if (id == BT_NEXT && (page + 1) * ITEMS_PER_PAGE < result_count())
            ++page;
        else if (id >= BT_ITEM_FIRST && id < BT_ITEM_FIRST + ITEMS_PER_PAGE)
        {
            size_t visible = id - BT_ITEM_FIRST;
            if (mode == Mode::CATALOG)
                request_purchase(visible);
            else
                request_transfer(visible);
        }
        refresh_labels();
        return Button::NORMAL;
    }

    void UICashShop::rebuild()
    {
        filtered.clear();
        const std::string query = search.get_text();
        const auto& catalog = model->catalog();
        for (size_t index = 0; index < catalog.size(); ++index)
        {
            const CashCommodity& item = catalog[index];
            bool gender_ok = item.gender == 2 || item.gender == (female ? 1 : 0);
            bool category_ok = category == 0 || item.category == category;
            bool query_ok = query.empty() || item.name.find(query) != std::string::npos ||
                std::to_string(item.sn).find(query) != std::string::npos;
            if (item.on_sale && gender_ok && category_ok && query_ok)
                filtered.push_back(index);
        }
        page = 0;
        refresh_labels();
    }

    void UICashShop::refresh_labels()
    {
        const CashBalances& balances = model->balances();
        balance_label.change_text(
            "NX " + format_amount(balances.nx_credit) + "  点数 " +
            format_amount(balances.maple_points) + "  预付 " +
            format_amount(balances.nx_prepaid));
        std::string mode_name = mode == Mode::CATALOG ? "商品" :
            mode == Mode::LOCKER ? "商城仓库" : "角色现金栏";
        status_label.change_text(mode_name + "  支付: " + currency_name(currency) +
            "  第 " + std::to_string(page + 1) + " 页");

        size_t total = result_count();
        size_t offset = page * ITEMS_PER_PAGE;
        size_t count = offset < total ? std::min(ITEMS_PER_PAGE, total - offset) : 0;
        for (size_t index = 0; index < ITEMS_PER_PAGE; ++index)
        {
            bool active = index < count;
            buttons[BT_ITEM_FIRST + index]->set_active(active);
            item_names[index].change_text(active ? visible_name(index) : "");
            std::string price;
            if (active && mode == Mode::CATALOG)
            {
                const CashCommodity& item = model->catalog()[filtered[page * ITEMS_PER_PAGE + index]];
                price = format_amount(item.price) + " NX";
            }
            else if (active)
                price = mode == Mode::LOCKER ? "取出" : "存入";
            item_prices[index].change_text(price);
        }
    }

    void UICashShop::set_mode(Mode value)
    {
        mode = value;
        page = 0;
        refresh_labels();
    }

    void UICashShop::set_category(int8_t value)
    {
        category = value;
        mode = Mode::CATALOG;
        rebuild();
    }

    void UICashShop::request_purchase(size_t visible_index)
    {
        size_t index = page * ITEMS_PER_PAGE + visible_index;
        if (index >= filtered.size())
            return;
        const CashCommodity item = model->catalog()[filtered[index]];
        std::string question = "使用 " + std::string(currency_name(currency)) + " 购买 " +
            item.name + "，价格 " + format_amount(item.price) + "？";
        UI::get().emplace<UIYesNo>(question, [this, item](bool confirmed) {
            if (!confirmed)
                return;
            if (model->balances().get(currency) < item.price)
            {
                show_error("所选支付余额不足。");
                return;
            }
            set_pending(true);
            BuyCashItemPacket(item.sn, currency, item.package).dispatch();
        });
    }

    void UICashShop::request_transfer(size_t visible_index)
    {
        size_t index = page * ITEMS_PER_PAGE + visible_index;
        if (mode == Mode::LOCKER)
        {
            if (index >= model->locker().size())
                return;
            pending_cash_id = model->locker()[index].cash_id;
            set_pending(true);
            TakeCashLockerItemPacket(pending_cash_id).dispatch();
            return;
        }
        std::vector<CashInventoryItem> items = Stage::get().get_player().get_inventory().get_cash_items();
        if (index >= items.size())
            return;
        pending_cash_id = items[index].cash_id;
        set_pending(true);
        PutCashLockerItemPacket(pending_cash_id, static_cast<int8_t>(items[index].type)).dispatch();
    }

    void UICashShop::set_pending(bool value)
    {
        pending = value;
        for (size_t index = 0; index < ITEMS_PER_PAGE; ++index)
            buttons[BT_ITEM_FIRST + index]->set_active(
                !pending && page * ITEMS_PER_PAGE + index < result_count());
    }

    size_t UICashShop::result_count() const
    {
        size_t total = mode == Mode::CATALOG ? filtered.size() :
            mode == Mode::LOCKER ? model->locker().size() :
            Stage::get().get_player().get_inventory().get_cash_items().size();
        return total;
    }

    std::string UICashShop::visible_name(size_t index) const
    {
        size_t absolute = page * ITEMS_PER_PAGE + index;
        if (mode == Mode::CATALOG)
            return model->catalog()[filtered[absolute]].name;
        if (mode == Mode::LOCKER)
            return CashShopModel::item_name(model->locker()[absolute].item_id);
        auto items = Stage::get().get_player().get_inventory().get_cash_items();
        return CashShopModel::item_name(items[absolute].item_id);
    }
}
