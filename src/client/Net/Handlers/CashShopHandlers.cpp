#include "CashShopHandlers.h"

#include "Helpers/ItemParser.h"
#include "Helpers/LoginParser.h"
#include "SetfieldHandlers.h"
#include "../../Console.h"
#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/CashShop/UICashShop.h"
#include "../Packets/LoginPackets.h"
#include "../Session.h"

#include <memory>

namespace jrc
{
    namespace
    {
        UICashShop* cash_shop_ui()
        {
            return UI::get().get_element<UICashShop>().get();
        }
    }

    void SetCashShopHandler::handle(InPacket& recv) const
    {
        recv.read_long();
        recv.read_byte();
        int32_t cid = recv.read_int();
        Player& player = Stage::get().get_player();
        if (player.get_oid() != cid)
            throw PacketError("Cash shop character id does not match current player");

        StatsEntry stats = LoginParser::parse_stats(recv);
        player.reset_progress(stats);
        SetfieldHandler().parse_character_data(recv, player);
        player.recalc_stats(true);

        recv.read_byte();
        recv.read_string();
        recv.read_int();

        auto model = std::make_shared<CashShopModel>();
        int16_t special_count = recv.read_short();
        if (special_count < 0)
            throw PacketError("Negative special cash item count");
        for (int16_t index = 0; index < special_count; ++index)
        {
            int32_t sn = recv.read_int();
            int32_t modifier = recv.read_int();
            int8_t info = recv.read_byte();
            model->apply_special_item(sn, modifier, info);
        }

        recv.skip(121);
        for (size_t index = 0; index < 8 * 2 * 5; ++index)
        {
            recv.read_int();
            recv.read_int();
            recv.read_int();
        }
        recv.read_int();
        recv.read_short();
        recv.read_byte();
        recv.read_int();

        Stage::get().clear();
        UI::get().enter_cash_shop(std::move(model), stats.female);
        UI::get().enable();
    }

    void QueryCashHandler::handle(InPacket& recv) const
    {
        CashBalances balances;
        balances.nx_credit = recv.read_int();
        balances.maple_points = recv.read_int();
        balances.nx_prepaid = recv.read_int();
        if (UICashShop* shop = cash_shop_ui())
            shop->set_balances(balances);
    }

    CashLockerItem CashShopOperationHandler::parse_locker_item(InPacket& recv, bool gift)
    {
        CashLockerItem item;
        item.cash_id = recv.read_long();
        if (gift)
        {
            item.item_id = recv.read_int();
            item.gift_from = recv.read_padded_string(13);
            recv.read_padded_string(73);
            return item;
        }
        recv.read_int();
        recv.read_int();
        item.item_id = recv.read_int();
        item.sn = recv.read_int();
        item.count = recv.read_short();
        item.gift_from = recv.read_padded_string(13);
        item.expiration = recv.read_long();
        recv.read_long();
        return item;
    }

    void CashShopOperationHandler::handle(InPacket& recv) const
    {
        uint8_t operation = static_cast<uint8_t>(recv.read_byte());
        UICashShop* shop = cash_shop_ui();
        switch (operation)
        {
        case 0x4B:
        {
            int16_t count = recv.read_short();
            if (count < 0)
                throw PacketError("Negative cash locker item count");
            std::vector<CashLockerItem> items;
            items.reserve(static_cast<size_t>(count));
            for (int16_t index = 0; index < count; ++index)
                items.push_back(parse_locker_item(recv, false));
            recv.read_short();
            recv.read_short();
            if (shop)
                shop->replace_locker(std::move(items));
            return;
        }
        case 0x4D:
        {
            int16_t count = recv.read_short();
            if (count < 0)
                throw PacketError("Negative cash gift count");
            for (int16_t index = 0; index < count; ++index)
                parse_locker_item(recv, true);
            return;
        }
        case 0x4F:
        case 0x55:
            for (size_t index = 0; index < 10; ++index)
                recv.read_int();
            return;
        case 0x57:
        {
            CashLockerItem item = parse_locker_item(recv, false);
            if (shop)
                shop->purchase_succeeded(std::move(item));
            return;
        }
        case 0x89:
        {
            uint8_t count = static_cast<uint8_t>(recv.read_byte());
            std::vector<CashLockerItem> items;
            items.reserve(count);
            for (uint8_t index = 0; index < count; ++index)
                items.push_back(parse_locker_item(recv, false));
            recv.read_short();
            if (shop)
                shop->package_purchase_succeeded(std::move(items));
            return;
        }
        case 0x5C:
        {
            std::string message = error_message(static_cast<uint8_t>(recv.read_byte()));
            if (shop)
                shop->show_error(message);
            return;
        }
        case 0x68:
        {
            int16_t slot = recv.read_short();
            const int64_t cash_id = shop ? shop->transfer_cash_id() : 0;
            ItemParser::parse_item_auto(
                recv, slot, Stage::get().get_player().get_inventory(), cash_id);
            if (shop)
                shop->take_succeeded();
            return;
        }
        case 0x6A:
        {
            CashLockerItem item = parse_locker_item(recv, false);
            if (shop)
                shop->put_succeeded(std::move(item));
            return;
        }
        default:
        {
            std::string message = "未支持的商城响应 0x";
            constexpr char hex[] = "0123456789ABCDEF";
            message.push_back(hex[(operation >> 4) & 0xF]);
            message.push_back(hex[operation & 0xF]);
            Console::get().print(message);
            if (shop)
                shop->show_error(message);
        }
        }
    }

    std::string CashShopOperationHandler::error_message(uint8_t code)
    {
        switch (code)
        {
        case 0xA3: return "商城请求超时，请重试。";
        case 0xA5: return "所选支付余额不足。";
        case 0xAA: return "该商品不适用于当前角色性别。";
        case 0xAC: return "商城仓库已满。";
        case 0xBB: return "角色物品栏空间不足。";
        case 0xBF: return "该商品当前不可购买。";
        case 0xC0: return "该商品库存不足。";
        case 0xC3: return "商城当前不可用。";
        case 0xCD: return "已达到商城每日购买限制。";
        case 0xE6: return "该商品不能使用 Maple Point 购买。";
        default: return "商城操作失败，错误码 " + std::to_string(code) + "。";
        }
    }

    void ChangeChannelHandler::handle(InPacket& recv) const
    {
        if (recv.read_byte() != 1)
        {
            if (UICashShop* shop = cash_shop_ui())
                shop->reconnect_failed("服务端拒绝切换频道");
            return;
        }
        std::string address;
        for (size_t index = 0; index < 4; ++index)
        {
            if (index > 0)
                address.push_back('.');
            address += std::to_string(static_cast<uint8_t>(recv.read_byte()));
        }
        std::string port = std::to_string(static_cast<uint16_t>(recv.read_short()));
        int32_t cid = Stage::get().get_player().get_oid();
        Session::get().reconnect(address.c_str(), port.c_str());
        if (Session::get().is_connected())
            PlayerLoginPacket(cid).dispatch();
        else if (UICashShop* shop = cash_shop_ui())
            shop->reconnect_failed(address + ":" + port);
    }
}
