#include "TradeHandlers.h"

#include "Helpers/LoginParser.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/Trade/UITrade.h"
#include "../../IO/UITypes/UIItemInventory.h"
#include "../../IO/UITypes/UINotice.h"
#include "../Packets/TradePackets.h"
#include "../Protocol/TradeProtocol.h"

#include <string>

namespace jrc
{
    namespace
    {
        enum Action : int8_t
        {
            INVITE = 2,
            VISIT = 4,
            ROOM = 5,
            CHAT = 6,
            EXIT = 10,
            SET_ITEMS = 15,
            SET_MESO = 16,
            CONFIRM = 17
        };

        void show_notice(const std::string& message)
        {
            UI::get().emplace<UIOk>(message, []() {});
        }

        std::string room_error(int8_t result)
        {
            switch (result)
            {
            case 1: return "交易房间已经关闭。";
            case 2: return "对方当前无法加入交易。";
            case 3: return "当前有其他请求正在处理中。";
            case 4: return "角色死亡时不能交易。";
            case 7: return "当前状态不能交易该物品。";
            case 12: return "双方不在同一地图，无法交易。";
            default: return "无法开始交易（错误 " + std::to_string(result) + "）。";
            }
        }

        std::string trade_result(int8_t result, bool completed)
        {
            if (completed || result == 7)
                return "交易成功。";
            switch (result)
            {
            case 2: return "对方取消了交易。";
            case 8: return "交易失败，请检查背包空间和金币上限。";
            case 9: return "交易失败：不能持有重复的唯一物品。";
            case 12: return "交易取消：双方已不在同一地图。";
            case 13: return "交易失败：客户端物品数据不一致。";
            default: return "交易已取消。";
            }
        }

        void ensure_inventory_open()
        {
            if (!UI::get().is_element_active(UIElement::ITEMINVENTORY))
            {
                UI::get().emplace<UIItemInventory>(
                    Stage::get().get_player().get_inventory());
            }
        }
    }

    void TradeHandler::handle(InPacket& recv) const
    {
        int8_t action = recv.read_byte();
        switch (action)
        {
        case INVITE:
        {
            recv.read_byte();
            std::string inviter = recv.read_string();
            if (recv.length() >= 4)
                recv.skip(4);
            UI::get().emplace<UIYesNo>(inviter + " 邀请你进行交易，是否接受？",
                [](bool accepted) {
                    TradeActionPacket(accepted ? TradeActionPacket::VISIT :
                        TradeActionPacket::DECLINE).dispatch();
                });
            break;
        }
        case ROOM:
        {
            int8_t room_type = recv.read_byte();
            if (room_type == 0)
            {
                show_notice(room_error(recv.read_byte()));
                return;
            }
            if (room_type != 3)
                return;

            recv.read_byte();
            uint8_t local_slot = static_cast<uint8_t>(recv.read_byte());
            if (auto existing = UI::get().get_element<UITrade>())
                existing->close();
            auto trade = UI::get().emplace<UITrade>(
                Stage::get().get_player().get_inventory(), local_slot);
            if (!trade)
                return;

            uint8_t slot = static_cast<uint8_t>(recv.read_byte());
            while (slot != 0xFF)
            {
                LookEntry look = LoginParser::parse_look(recv);
                trade->set_participant(slot, recv.read_string(), look);
                slot = static_cast<uint8_t>(recv.read_byte());
            }
            ensure_inventory_open();
            break;
        }
        case VISIT:
        {
            uint8_t slot = static_cast<uint8_t>(recv.read_byte());
            LookEntry look = LoginParser::parse_look(recv);
            std::string name = recv.read_string();
            if (auto trade = UI::get().get_element<UITrade>())
                trade->set_participant(slot, name, look);
            break;
        }
        case SET_ITEMS:
        {
            bool partner = recv.read_byte() != 0;
            uint8_t slot = static_cast<uint8_t>(recv.read_byte());
            trade_protocol::ItemSummary item = trade_protocol::parse_item(recv);
            if (auto trade = UI::get().get_element<UITrade>())
                trade->set_item(partner, slot, item.item_id, item.quantity);
            break;
        }
        case SET_MESO:
        {
            bool partner = recv.read_byte() != 0;
            int32_t amount = recv.read_int();
            if (auto trade = UI::get().get_element<UITrade>())
                trade->set_meso(partner, amount);
            break;
        }
        case CHAT:
            recv.read_byte();
            recv.read_byte();
            if (auto trade = UI::get().get_element<UITrade>())
                trade->add_chat(recv.read_string());
            break;
        case CONFIRM:
            if (auto trade = UI::get().get_element<UITrade>())
                trade->partner_confirmed();
            break;
        case EXIT:
        {
            recv.read_byte();
            int8_t result = recv.read_byte();
            bool completed = false;
            if (auto trade = UI::get().get_element<UITrade>())
            {
                completed = trade->both_confirmed();
                trade->close();
            }
            UI::get().remove(UIElement::TRADE);
            show_notice(trade_result(result, completed));
            UI::get().enable();
            break;
        }
        default:
            break;
        }
    }
}
