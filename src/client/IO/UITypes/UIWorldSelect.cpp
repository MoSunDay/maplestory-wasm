//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UIWorldSelect.h"
#include "../UiAssetVersion.h"
#include "WorldSelectPolicy.h"

#include "../../Console.h"
#include "../../Graphics/Sprite.h"
#include "../../IO/UI.h"
#include "../../IO/Components/MapleButton.h"
#include "../../IO/Components/TwoSpriteButton.h"
#include "../../Net/Session.h"
#include "../../Net/Packets/LoginPackets.h"

#include "nlnx/nx.hpp"

#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#endif

namespace jrc
{
    namespace
    {
        constexpr Point<int16_t> CHANNEL_WINDOW_CENTER{ 200, 170 };
        // Pre-Chaos (v83) layout, derived from the WorldSelect node
        // inventory: chBackgrn 449x233 origin (224,116), channel plates
        // 92x30, BtGoworld 140x41, world signs 26x95.
        constexpr Point<int16_t> PRECH_WORLD_SIGN_POS{ 200, 10 };
        constexpr Point<int16_t> PRECH_ENTER_POS{ 270, 228 };
        constexpr Point<int16_t> PRECH_CHANNEL_POS{ 100, 100 };
        constexpr int16_t PRECH_CHANNEL_STEP_Y = 36;
    }

    UIWorldSelect::UIWorldSelect(std::vector<World> worlds, uint8_t worldcount)
        : UIElement({ 0, 0 }, { 800, 600 }),
          worldid(0),
          channelid(world_select::selected_channel_id()),
          channelcount(0),
          request_pending(false) {

        nl::node back = nl::nx::map["Back"]["login.img"]["back"];
        nl::node worldsel = nl::nx::ui["Login.img"]["WorldSelect"];
        nl::node worldsrc = worldsel["BtWorld"]["release"];
        nl::node channelsrc = worldsel["BtChannel"];
        nl::node frame = nl::nx::ui["Login.img"]["Common"]["frame"];
        // Same probe as the NxFiles version warning: pre-Chaos UI.nx stores
        // the channel window as WorldSelect/chBackgrn with per-channel
        // bitmaps under WorldSelect/channel, not in the BtChannel subtree.
        const bool post_chaos = static_cast<bool>(channelsrc["layer:bg"]);

        // Same 11 -> 0 fallback as the other login screens: some UI.nx
        // variants do not ship the login backdrop under back/11.
        if (back["11"])
        {
            sprites.emplace_back(back["11"], Point<int16_t>(370, 300));
        }
        else if (back["0"])
        {
            sprites.emplace_back(back["0"], Point<int16_t>(370, 300));
        }
        if (post_chaos)
            sprites.emplace_back(worldsrc["layer:bg"], Point<int16_t>(650, 45));
        sprites.emplace_back(frame, Point<int16_t>(400, login_frame_y()));

        if (post_chaos)
        {
            buttons[BT_ENTERWORLD] = std::make_unique<MapleButton>(
                channelsrc["button:GoWorld"],
                CHANNEL_WINDOW_CENTER
                );
        }
        else
        {
            buttons[BT_ENTERWORLD] = std::make_unique<MapleButton>(
                worldsel["BtGoworld"],
                PRECH_ENTER_POS
                );
        }

        if (worldcount <= 0 || worlds.empty())
        {
            buttons[BT_ENTERWORLD]->set_state(Button::DISABLED);
            return;
        }

        // This screen currently renders one world entry. Bind it to the
        // server-provided id instead of a stale saved setting, which may refer
        // to a world that is no longer available. Use the LAST announced
        // world: a serverlist burst may arrive as one coalesced delivery
        // (all worlds here) or as one packet per world (each packet
        // re-emplacing this screen, the final one surviving). Both
        // delivery orders then resolve to the same world id.
        const World& world = worlds.back();
        worldid = static_cast<uint8_t>(world.wid);
        channelcount = world_select::selectable_channel_count(world.channelcount);

        if (post_chaos)
        {
            buttons[BT_WORLD0] = std::make_unique<MapleButton>(worldsrc["button:15"], Point<int16_t>(650, 20));
        }
        else
        {
            nl::node sign = worldsel["BtWorld"][std::to_string(worldid)];
            if (!sign)
                sign = worldsel["BtWorld"]["0"];
            buttons[BT_WORLD0] = std::make_unique<MapleButton>(sign, PRECH_WORLD_SIGN_POS);
        }
        buttons[BT_WORLD0]->set_state(Button::PRESSED);

        if (post_chaos)
        {
            sprites.emplace_back(channelsrc["layer:bg"], CHANNEL_WINDOW_CENTER);
            sprites.emplace_back(channelsrc["release"]["layer:15"], CHANNEL_WINDOW_CENTER);
        }
        else
        {
            sprites.emplace_back(worldsel["chBackgrn"], CHANNEL_WINDOW_CENTER);
        }

        for (uint8_t i = 0; i < channelcount; ++i)
        {
            if (post_chaos)
            {
                nl::node chnode = channelsrc["button:" + std::to_string(i)];
                buttons[BT_CHANNEL0 + i] = std::make_unique<TwoSpriteButton>(
                    chnode["normal"]["0"], chnode["keyFocused"]["0"],
                    CHANNEL_WINDOW_CENTER
                    );
            }
            else
            {
                // v83 channel plates only carry normal/disabled bitmaps, so
                // the normal plate doubles as the focus state.
                nl::node chnode = worldsel["channel"][std::to_string(i)];
                buttons[BT_CHANNEL0 + i] = std::make_unique<TwoSpriteButton>(
                    chnode["normal"], chnode["normal"],
                    PRECH_CHANNEL_POS
                        + Point<int16_t>(0, PRECH_CHANNEL_STEP_Y * i)
                    );
            }
            if (i == channelid)
                buttons[BT_CHANNEL0 + i]->set_state(Button::PRESSED);
        }

        if (channelcount == 0)
            buttons[BT_ENTERWORLD]->set_state(Button::DISABLED);
    }

    void UIWorldSelect::draw(float alpha) const
    {
        UIElement::draw(alpha);
    }

    uint8_t UIWorldSelect::get_world_id() const
    {
        return worldid;
    }

    uint8_t UIWorldSelect::get_channel_id() const
    {
        return channelid;
    }

    void UIWorldSelect::doubleclick(Point<int16_t> cursorpos)
    {
        for (uint8_t i = 0; i < channelcount; ++i)
        {
            if (buttons[BT_CHANNEL0 + i]->bounds(position).contains(cursorpos))
            {
                channelid = i;
                enter_selected_channel();
                return;
            }
        }
    }

    bool UIWorldSelect::enter_selected_channel()
    {
        if (request_pending)
            return true;

        if (channelcount == 0 || channelid >= channelcount)
            return false;

        if (!Session::get().is_connected())
        {
            Console::get().print("Cannot enter channel: login connection is closed");
#ifdef MS_PLATFORM_WASM
            EM_ASM({
                if (window.MapleConnectionLost)
                {
                    window.MapleConnectionLost.show('登录连接已断开，请刷新页面后重试。');
                }
            });
#endif
            return false;
        }

        // Both GLFW mouse-down and the browser compatibility bridge can see
        // one physical click. Mark the request first so only one packet leaves.
        world_select::begin_request(request_pending);
        UI::get().disable();
        CharlistRequestPacket(worldid, channelid).dispatch();
        return true;
    }

    void UIWorldSelect::recover_charlist_request()
    {
        world_select::recover_request(request_pending);
        makeactive();
        if (channelcount > 0)
        {
            buttons[BT_ENTERWORLD]->set_state(Button::NORMAL);
        }
    }

    Button::State UIWorldSelect::button_pressed(uint16_t id)
    {
        if (id == BT_ENTERWORLD)
        {
            return enter_selected_channel() ? Button::PRESSED : Button::DISABLED;
        }
        else if (id == BT_WORLD0)
        {
            // There is only one rendered world button, so selecting it must
            // preserve the actual server world id rather than deriving an id
            // from the local button index.
            return Button::PRESSED;
        }
        else if (id >= BT_CHANNEL0 && id < BT_CHANNEL0 + channelcount)
        {
            buttons[BT_CHANNEL0 + channelid]->set_state(Button::NORMAL);
            channelid = static_cast<uint8_t>(id - BT_CHANNEL0);
            return Button::PRESSED;
        }

        return Button::DISABLED;
    }
}
