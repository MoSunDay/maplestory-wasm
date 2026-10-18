#include "CharacterInfoHandlers.h"

#include "CharacterInfoProtocol.h"
#include "../PacketError.h"

#include "../../IO/CharacterInfo/UIUserInfo.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIChatBar.h"
#include "../../IO/UITypes/UIStatusBar.h"

#include <utility>

namespace jrc
{
    void CharacterInfoHandler::handle(InPacket& packet) const
    {
        try
        {
            character_info::CharacterInfoPayload payload =
                character_info::parse_character_info(packet);
            if (auto window = UI::get().get_element<UIUserInfo>())
                window->apply_info(std::move(payload));
        }
        catch (const PacketError&)
        {
            if (auto window = UI::get().get_element<UIUserInfo>())
                window->fail_info("人物信息数据异常，请重新打开。");
            throw;
        }
    }

    void FameResponseHandler::handle(InPacket& packet) const
    {
        character_info::FameResponse response;
        try
        {
            response = character_info::parse_fame_response(packet);
        }
        catch (const PacketError&)
        {
            if (auto window = UI::get().get_element<UIUserInfo>())
                window->fail_fame("人气响应数据异常，请稍后重试。");
            throw;
        }
        if (response.status == character_info::FameStatus::RECEIVED)
        {
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                const std::string direction = response.increase ? "增加" : "降低";
                statusbar->send_chatline(response.character_name +
                    " 将你的人气" + direction + "了。", UIChatbar::YELLOW);
            }
            return;
        }

        if (auto window = UI::get().get_element<UIUserInfo>())
            window->apply_fame_response(response);
    }
}
