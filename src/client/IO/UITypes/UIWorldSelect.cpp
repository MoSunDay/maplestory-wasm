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

#include "../../Configuration.h"
#include "../../Graphics/Sprite.h"
#include "../../IO/UI.h"
#include "../../IO/Components/MapleButton.h"
#include "../../IO/Components/TwoSpriteButton.h"
#include "../../Net/Packets/LoginPackets.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace
    {
        constexpr Point<int16_t> CHANNEL_WINDOW_TOP{ 200, 20 };
    }

    UIWorldSelect::UIWorldSelect(std::vector<World> worlds, uint8_t worldcount)
        : UIElement({ 0, 0 }, { 800, 600 }), channelcount(0) {

        channelid = Setting<DefaultChannel>::get().load();

        nl::node back = nl::nx::map["Back"]["login.img"]["back"];
        nl::node worldsrc = nl::nx::ui["Login.img"]["WorldSelect"]["BtWorld"]["release"];
        nl::node channelsrc = nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"];
        nl::node frame = nl::nx::ui["Login.img"]["Common"]["frame"];

        sprites.emplace_back(back["11"], Point<int16_t>(370, 300));
        sprites.emplace_back(worldsrc["layer:bg"], Point<int16_t>(650, 45));
        sprites.emplace_back(frame, Point<int16_t>(400, 300));

        buttons[BT_ENTERWORLD] = std::make_unique<MapleButton>(
            channelsrc["button:GoWorld"],
            CHANNEL_WINDOW_TOP
            );

        if (worldcount <= 0 || worlds.empty())
        {
            buttons[BT_ENTERWORLD]->set_state(Button::DISABLED);
            return;
        }

        const World& world = worlds.front();
        // This screen currently renders one world entry. Bind it to the
        // server-provided id instead of a stale saved setting, which may refer
        // to a world that is no longer available.
        worldid = static_cast<uint8_t>(world.wid);
        channelcount = world.channelcount;

        buttons[BT_WORLD0] = std::make_unique<MapleButton>(worldsrc["button:15"], Point<int16_t>(650, 20));
        buttons[BT_WORLD0]->set_state(Button::PRESSED);

        sprites.emplace_back(channelsrc["layer:bg"], CHANNEL_WINDOW_TOP);
        sprites.emplace_back(channelsrc["release"]["layer:15"], CHANNEL_WINDOW_TOP);

        if (channelid >= world.channelcount)
            channelid = 0;

        for (uint8_t i = 0; i < channelcount; ++i)
        {
            nl::node chnode = channelsrc["button:" + std::to_string(i)];
            buttons[BT_CHANNEL0 + i] = std::make_unique<TwoSpriteButton>(
                chnode["normal"]["0"], chnode["keyFocused"]["0"],
                CHANNEL_WINDOW_TOP
                );
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

    Button::State UIWorldSelect::button_pressed(uint16_t id)
    {
        if (id == BT_ENTERWORLD)
        {
            if (channelcount == 0 || channelid >= channelcount)
                return Button::DISABLED;

            UI::get().disable();

            CharlistRequestPacket(worldid, channelid)
                .dispatch();

            return Button::PRESSED;
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
