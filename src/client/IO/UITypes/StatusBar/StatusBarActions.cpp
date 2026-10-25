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
// Statusbar popup dispatch, split from UIStatusBar.cpp to keep that file
// within its size budget. The declaration stays on UIStatusbar.
#include "../UIStatusBar.h"

#include "../../UI.h"
#include "../Pet/UIPetOptions.h"
#include "../Settings/UIGameOptions.h"

#include "../../../Console.h"
#include "../../../Gameplay/Stage.h"
#include "../../../Net/Packets/LoginPackets.h"
#include "../../../Net/Session.h"

namespace jrc
{
    void UIStatusbar::dispatch_popup_action(StatusBarPopup::Action action)
    {
        popup.close();
        switch (action)
        {
        case StatusBarPopup::Action::STATS:
            UI::get().send_menu(KeyAction::CHARSTATS);
            break;
        case StatusBarPopup::Action::EQUIPS:
            UI::get().send_menu(KeyAction::EQUIPS);
            break;
        case StatusBarPopup::Action::ITEMS:
            UI::get().send_menu(KeyAction::INVENTORY);
            break;
        case StatusBarPopup::Action::SKILLS:
            UI::get().send_menu(KeyAction::SKILLBOOK);
            break;
        case StatusBarPopup::Action::QUESTS:
            UI::get().send_menu(KeyAction::QUESTLOG);
            break;
        case StatusBarPopup::Action::KEY_CONFIG:
            UI::get().send_menu(KeyAction::KEYCONFIG);
            break;
        case StatusBarPopup::Action::PET_OPTIONS:
            UI::get().emplace<UIPetOptions>();
            break;
        case StatusBarPopup::Action::GAME_OPTIONS:
            UI::get().emplace<UIGameOptions>();
            break;
        case StatusBarPopup::Action::LOGOUT:
        {
            PlayerDisconnectPacket().dispatch();
            Session::get().disconnect();
            const Error reconnect_error = Session::get().init();
            Stage::get().clear();
            UI::get().change_state(UI::LOGIN);
            UI::get().enable();
            if (reconnect_error != Error::NONE)
            {
                Console::get().print("Failed to reconnect to the login server after logout");
            }
            return;
        }
        case StatusBarPopup::Action::NONE:
            break;
        }
    }
}
