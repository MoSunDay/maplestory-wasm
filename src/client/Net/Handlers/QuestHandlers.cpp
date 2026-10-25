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
#include "QuestHandlers.h"

#include "../../Character/Char.h"
#include "../../Console.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/Animation.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <chrono>
#include <string>

namespace
{
    int64_t now_seconds()
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now);
        return seconds.count();
    }

    void complete_quest(int16_t quest_id, const char* source)
    {
        jrc::Stage::get().get_player().get_quests().complete(
            quest_id,
            now_seconds()
        );
        jrc::Console::get().print(
            std::string(source) + ": quest " + std::to_string(quest_id)
            + " completed"
        );
    }
}

namespace jrc
{
    void QuestClearHandler::handle(InPacket& recv) const
    {
        const int16_t quest_id = recv.read_short();
        complete_quest(quest_id, "[QuestClear]");
    }

    void SetQuestClearHandler::handle(InPacket& recv) const
    {
        // Never sent by this server; keep the parse lenient so a stray
        // packet cannot desync the stream (PacketError is caught upstream).
        const int16_t quest_id = recv.read_short();
        complete_quest(quest_id, "[SetQuestClear]");
    }

    void UpdateQuestInfoHandler::handle(InPacket& recv) const
    {
        const int8_t mode = recv.read_byte();
        switch (mode)
        {
        case 6:
        {
            // Quest timer list: count x (quest id, remaining ms).
            const int16_t count = recv.read_short();
            for (int16_t i = 0; i < count; ++i)
            {
                recv.read_short();
                recv.read_int();
            }
            Console::get().print(
                "[UpdateQuestInfo] Timers updated for " +
                std::to_string(count) + " quest(s)"
            );
            break;
        }
        case 7:
        {
            // Npc marker list: count x quest id.
            const int16_t count = recv.read_short();
            for (int16_t i = 0; i < count; ++i)
            {
                recv.read_short();
            }
            Console::get().print(
                "[UpdateQuestInfo] Npc markers updated for " +
                std::to_string(count) + " quest(s)"
            );
            break;
        }
        case 8:
        {
            // Quest npc location hint. Trailing bytes differ between the
            // server variants, so they are deliberately not read.
            const int16_t quest_id = recv.read_short();
            const int32_t npc_id = recv.read_int();
            Console::get().print(
                "[UpdateQuestInfo] Quest " + std::to_string(quest_id) +
                " npc " + std::to_string(npc_id) + " location updated"
            );
            break;
        }
        case 0x0A:
        {
            const int16_t quest_id = recv.read_short();
            Console::get().print(
                "[UpdateQuestInfo] Notice for quest " +
                std::to_string(quest_id)
            );
            break;
        }
        case 0x0B:
        case 0x0D:
        case 0x0E:
            // No payload.
            Console::get().print("[UpdateQuestInfo] Quest notice updated");
            break;
        case 0x0F:
        {
            // Expired: the quest is forfeited.
            const int16_t quest_id = recv.read_short();
            Stage::get().get_player().get_quests().forfeit(quest_id);
            Console::get().print(
                "[UpdateQuestInfo] Quest " + std::to_string(quest_id) +
                " expired and was forfeited"
            );
            break;
        }
        default:
            Console::get().print(
                "[UpdateQuestInfo] Unknown mode " + std::to_string(mode)
            );
            break;
        }
    }

    void ShowItemEffectHandler::handle(InPacket& recv) const
    {
        const int32_t character_id = recv.read_int();
        const int32_t item_id = recv.read_int();

        const nl::node source =
            nl::nx::effect["ItemEff.img"][std::to_string(item_id)];
        if (!static_cast<bool>(source))
        {
            Console::get().print(
                "[ShowItemEffect] No effect for item " +
                std::to_string(item_id)
            );
            return;
        }

        // get_character() resolves the local player as well as foreign
        // characters spawned on the map.
        if (Optional<Char> character = Stage::get().get_character(character_id))
        {
            character->show_attack_effect(Animation(source), 0);
        }
        else
        {
            Console::get().print(
                "[ShowItemEffect] Character " +
                std::to_string(character_id) + " not on the map"
            );
        }
    }
}
