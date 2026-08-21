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
#include <Net/Packets/GameplayPackets.h>
#include "SetfieldHandlers.h"

#include "Helpers/CharacterDataParser.h"
#include "Helpers/ItemParser.h"
#include "Helpers/LoginParser.h"

#include "../../Console.h"
#include "../../Constants.h"
#include "../../Timer.h"
#include "../../Audio/Audio.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UICharSelect.h"
#include "../../IO/Field/UIFieldClock.h"
#include "../../IO/Window.h"


namespace jrc
{
    void SetfieldHandler::transition(int32_t mapid, uint8_t portalid) const
    {
        float fadestep = 0.025f;
        Window::get().fadeout(fadestep, [mapid, portalid](){
            GraphicsGL::get().clear();
            Stage::get().load(mapid, portalid, [](){
                PlayerUpdatePacket().dispatch();
                UI::get().enable();
            });
            Timer::get().start();
            GraphicsGL::get().unlock();
        });

        GraphicsGL::get().lock();
        UI::get().disable();
        Stage::get().clear();
        Timer::get().start();
    }

    void SetfieldHandler::handle(InPacket& recv) const
    {
        recv.read_int(); // channel
        int8_t  mode1   = recv.read_byte();
        int8_t  mode2   = recv.read_byte();

        if (mode1 == 0 && mode2 == 0)
        {
            change_map(recv);
        }
        else
        {
            set_field(recv);
        }
    }

    void SetfieldHandler::change_map(InPacket& recv) const
    {
        recv.skip(3);

        int32_t mapid = recv.read_int();
        auto portalid = static_cast<uint8_t>(recv.read_byte());

        transition(mapid, portalid);
    }

    void SetfieldHandler::set_field(InPacket& recv) const
    {
        recv.skip(23);

        int32_t cid = recv.read_int();

        auto charselect = UI::get().get_element<UICharSelect>();
        bool returning_from_cash_shop = UI::get().get_state() == UI::CASHSHOP;
        if (!charselect && !returning_from_cash_shop)
        {
            // The character-select screen is gone (e.g. after a reconnect
            // race). Re-enable the UI rather than leaving it frozen.
            Console::get().print("set_field: character-select UI not found");
            UI::get().enable();
            return;
        }

        if (charselect)
        {
            const CharEntry& playerentry = charselect->get_character(cid);
            if (playerentry.cid != cid)
            {
                Console::get().print("set_field: cid mismatch, cannot enter game");
                UI::get().enable();
                return;
            }
            Stage::get().loadplayer(playerentry);
        }
        else if (Stage::get().get_player().get_oid() != cid)
        {
            Console::get().print("set_field: cash shop return cid mismatch");
            UI::get().enable();
            return;
        }

        Player& player = Stage::get().get_player();
        player.reset_progress(LoginParser::parse_stats(recv));

        parse_character_data(recv, player);

        player.recalc_stats(true);

        uint8_t portalid = player.get_stats().get_portal();
        int32_t mapid    = player.get_stats().get_mapid();

        transition(mapid, portalid);

        Sound(Sound::GAMESTART).play();
        UI::get().change_state(UI::GAME);
    }

    void SetfieldHandler::parse_character_data(InPacket& recv, Player& player) const
    {

        recv.read_byte(); // 'buddycap'
        if (recv.read_bool())
        {
            recv.read_string(); // 'linkedname'
        }

        parse_inventory(recv, player.get_inventory());
        parse_skillbook(recv, player.get_skills());
        parse_cooldowns(recv, player);
        parse_questlog(recv, player.get_quests());
        CharacterDataParser::parse_minigame_info(recv);
        CharacterDataParser::parse_ring_info(recv);
        parse_telerock(recv, player.get_telerock());
        parse_monsterbook(recv, player.get_monsterbook());
        CharacterDataParser::parse_new_year_info(recv);
        parse_areainfo(recv);

        recv.read_short(); // trailing character-info marker
    }

    void SetfieldHandler::parse_inventory(InPacket& recv, Inventory& invent) const
    {
        invent.set_meso(recv.read_int());
        invent.set_slotmax(InventoryType::EQUIP, static_cast<uint8_t>(recv.read_byte()));
        invent.set_slotmax(InventoryType::USE,   static_cast<uint8_t>(recv.read_byte()));
        invent.set_slotmax(InventoryType::SETUP, static_cast<uint8_t>(recv.read_byte()));
        invent.set_slotmax(InventoryType::ETC,   static_cast<uint8_t>(recv.read_byte()));
        invent.set_slotmax(InventoryType::CASH,  static_cast<uint8_t>(recv.read_byte()));

        recv.skip(8);

        for (size_t i = 0; i < 3; ++i)
        {
            InventoryType::Id inv =
                i == 0 ?
                    InventoryType::EQUIPPED :
                    InventoryType::EQUIP;
            int16_t pos = recv.read_short();
            while (pos != 0)
            {
                int16_t slot = i == 1 ? -pos : pos;
                ItemParser::parse_item(recv, inv, slot, invent);
                pos = recv.read_short();
            }
        }

        recv.skip(2);

        InventoryType::Id toparse[4] =
        {
            InventoryType::USE, InventoryType::SETUP,
            InventoryType::ETC, InventoryType::CASH
        };

        for (auto inv : toparse)
        {
            int8_t pos = recv.read_byte();
            while (pos != 0)
            {
                ItemParser::parse_item(recv, inv, pos, invent);
                pos = recv.read_byte();
            }
        }
    }

    void SetfieldHandler::parse_skillbook(InPacket& recv, Skillbook& skills) const
    {
        int16_t size = recv.read_short();
        for (int16_t i = 0; i < size; ++i)
        {
            int32_t skill_id = recv.read_int();
            int32_t level = recv.read_int();
            int64_t expiration = recv.read_long();
            bool fourthtjob = ((skill_id % 100000) / 10000 == 2);
            int32_t masterlevel = fourthtjob ? recv.read_int() : 0;
            skills.set_skill(skill_id, level, masterlevel, expiration);
        }
    }

    void SetfieldHandler::parse_cooldowns(InPacket& recv, Player& player) const
    {
        int16_t size = recv.read_short();
        for (int16_t i = 0; i < size; ++i)
        {
            int32_t skill_id = recv.read_int();
            int32_t cooltime = recv.read_short();
            player.add_cooldown(skill_id, cooltime);
        }
    }

    void SetfieldHandler::parse_questlog(InPacket& recv, Questlog& quests) const
    {
        int16_t size = recv.read_short();
        for (int16_t i = 0; i < size; ++i)
        {
            int16_t qid = recv.read_short();
            std::string qdata = recv.read_string();
            if (quests.is_started(qid))
            {
                int16_t qidl = quests.get_last_started();
                quests.add_in_progress(qidl, qid, qdata);
                i--;
            }
            else
            {
                quests.add_started(qid, qdata);
            }
        }

        std::map<int16_t, int64_t> completed = {};
        size = recv.read_short();
        for (int16_t i = 0; i < size; ++i)
        {
            int16_t qid  = recv.read_short();
            int64_t time = recv.read_long();
            quests.add_completed(qid, time);
        }
    }

    void SetfieldHandler::parse_monsterbook(InPacket& recv, Monsterbook& monsterbook) const
    {
        monsterbook.set_cover(recv.read_int());

        recv.skip(1);

        int16_t size = recv.read_short();
        for (int16_t i = 0; i < size; ++i)
        {
            int16_t cid = recv.read_short();
            int8_t mblv = recv.read_byte();

            monsterbook.add_card(cid, mblv);
        }
    }

    void SetfieldHandler::parse_telerock(InPacket& recv, Telerock& trock) const
    {
        for (size_t i = 0; i < 5; ++i)
        {
            trock.addlocation(recv.read_int());
        }

        for (size_t i = 0; i < 10; ++i)
        {
            trock.addviplocation(recv.read_int());
        }
    }

    void SetfieldHandler::parse_areainfo(InPacket& recv) const
    {
        std::map<int16_t, std::string> areainfo = {};
        int16_t arsize = recv.read_short();
        for (int16_t i = 0; i < arsize; ++i)
        {
            int16_t area   = recv.read_short();
            areainfo[area] = recv.read_string();
        }
    }

    void FieldEffectHandler::handle(InPacket& recv) const
    {
        int8_t mode = recv.read_byte();
        if (mode == 2)
        {
            std::string name = recv.read_string();
            if (name.empty())
            {
                Console::get().print("[FieldEffectHandler] Received empty object name for mode 2");
                return;
            }

            const size_t changed = Stage::get().set_named_object_active(name, false);
            if (changed == 0)
            {
                Console::get().print(
                    "[FieldEffectHandler] Named field object not found: " + name +
                    ", map: " + std::to_string(Stage::get().get_mapid())
                );
            }
            return;
        }

        if (mode == 3)
        {
            std::string path = recv.read_string();
            if (path.empty())
            {
                Console::get().print("[FieldEffectHandler] Received empty field effect path for mode 3");
                return;
            }

            Stage::get().add_effect(path);
            return;
        }

        if (mode == 4)
        {
            std::string path = recv.read_string();
            if (!Sound::play_field(path))
            {
                Console::get().print(
                    "[FieldEffectHandler] Field sound could not be resolved: " + path
                );
            }
            return;
        }

        Console::get().print(
            "[FieldEffectHandler] Unhandled field effect mode: " + std::to_string(mode) +
            ", remaining bytes: " + std::to_string(recv.length())
        );
    }

    void ClockHandler::handle(InPacket& recv) const
    {
        auto clock = UI::get().get_element<UIFieldClock>();
        if (!clock)
        {
            Console::get().print("[ClockHandler] Gameplay clock UI is unavailable");
            return;
        }

        const int8_t type = recv.read_byte();
        if (type == 1)
        {
            const auto hours = static_cast<uint8_t>(recv.read_byte());
            const auto minutes = static_cast<uint8_t>(recv.read_byte());
            const auto seconds = static_cast<uint8_t>(recv.read_byte());
            clock->set_wall_clock(hours, minutes, seconds);
            return;
        }
        if (type == 2)
        {
            clock->set_countdown(recv.read_int());
            return;
        }

        Console::get().print(
            "[ClockHandler] Unsupported clock type: " + std::to_string(type) +
            ", remaining bytes: " + std::to_string(recv.length())
        );
    }

    void StopClockHandler::handle(InPacket&) const
    {
        if (auto clock = UI::get().get_element<UIFieldClock>())
        {
            clock->stop();
        }
    }
}
