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
#include "QuestInteraction.h"

#include "QuestCheck.h"
#include "MapleStat.h"

#include "../Gameplay/Stage.h"
#include "../IO/UI.h"
#include "../IO/UITypes/Quest/UIQuestDialog.h"
#include "../IO/UITypes/UINpcTalk.h"
#include "../IO/UITypes/UIShop.h"

#include "Inventory/Inventory.h"
#include "Inventory/InventoryType.h"

#include <chrono>

namespace jrc
{
    namespace
    {
        // How long to wait for a server NPC dialog before deciding the
        // quest offer locally.
        constexpr auto NPC_TALK_TIMEOUT = std::chrono::milliseconds(600);

        // Single-slot arm state as function-local statics (the client UI
        // loop is single threaded).
        bool& armed()
        {
            static bool value = false;
            return value;
        }

        int32_t& armed_npc()
        {
            static int32_t value = 0;
            return value;
        }

        std::chrono::steady_clock::time_point& armed_at()
        {
            static std::chrono::steady_clock::time_point value{};
            return value;
        }

        int32_t& armed_map()
        {
            static int32_t value = 0;
            return value;
        }

        // Pure guard: an armed talk belongs to the field it was armed on,
        // so a map change between arm and timeout must not fire the offer
        // against an npc of the new map.
        bool armed_map_current(int32_t armed_mapid, int32_t field_mapid)
        {
            return armed_mapid == field_mapid;
        }

        // A server-driven npc dialog or shop already replaced the talk, so
        // the fallback must not fire on top of it.
        bool server_dialog_open()
        {
            if (auto npctalk = UI::get().get_element<UINpcTalk>())
            {
                if (npctalk->is_active())
                    return true;
            }

            if (auto shop = UI::get().get_element<UIShop>())
            {
                if (shop->is_active())
                    return true;
            }

            return false;
        }

        // Total count of one item across its bag (reuses the shared
        // InventoryType::by_item_id id-range mapping).
        int32_t count_item(int32_t item_id)
        {
            Inventory& inventory =
                Stage::get().get_player().get_inventory();
            const InventoryType::Id type =
                InventoryType::by_item_id(item_id);

            int32_t total = 0;
            const int16_t slotmax =
                static_cast<int16_t>(inventory.get_slotmax(type));
            for (int16_t slot = 1; slot <= slotmax; ++slot)
            {
                if (inventory.get_item_id(type, slot) == item_id)
                {
                    total += inventory.get_item_count(type, slot);
                }
            }
            return total;
        }
    }

    namespace quest_interaction
    {
        // Player context for quest_offer_at: level, job, meso and the
        // wall clock plus an item counter over the live inventory.
        // Lives in the quest_interaction namespace so every client-side
        // quest-offer probe (the fallback dialog and the npc quest
        // marker seam) resolves offers against the same live state.
        QuestCheckContext make_check_context()
        {
            Player& player = Stage::get().get_player();

            QuestCheckContext context;
            context.level = static_cast<uint16_t>(
                player.get_stats().get_stat(Maplestat::LEVEL));
            context.job = player.get_stats().get_job().get_id();
            context.meso = player.get_inventory().get_meso();
            // Wall-clock epoch ms: the repeat-interval check compares
            // against the server's completion timestamps.
            context.now_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            context.item_count = [](int32_t item_id)
            {
                return count_item(item_id);
            };
            return context;
        }

        void arm_npc_talk(int32_t npc_id)
        {
            armed() = true;
            armed_npc() = npc_id;
            armed_map() = Stage::get().get_mapid();
            armed_at() = std::chrono::steady_clock::now();
        }

        void cancel_npc_talk()
        {
            armed() = false;

            // The server dialog/shop owns the screen once it really opens:
            // if the fallback quest dialog already fired in a slow-network
            // race, drop it now so two dialogs do not stack (a YES accepted
            // flow is unaffected, its dialog is inactive by then).
            if (auto dialog = UI::get().get_element<UIQuestDialog>())
            {
                if (dialog->is_active())
                {
                    UI::get().remove(UIElement::QUESTDIALOG);
                }
            }
        }

        void update()
        {
            // Check-on-use guard: the player left the field the talk was
            // armed on, so the slot is stale — drop it instead of firing
            // an old-map offer on the new map.
            if (armed()
                && !armed_map_current(
                    armed_map(), Stage::get().get_mapid()))
            {
                armed() = false;
            }
            if (!armed())
            {
                return;
            }

            const auto elapsed =
                std::chrono::steady_clock::now() - armed_at();
            if (elapsed < NPC_TALK_TIMEOUT)
            {
                return;
            }

            const int32_t npc_id = armed_npc();
            armed() = false;

            if (server_dialog_open())
            {
                return;
            }

            Player& player = Stage::get().get_player();
            const QuestCheckContext context = make_check_context();
            QuestOffer offer{};
            if (!quest_offer_at(npc_id, player.get_quests(), context, offer))
            {
                return;
            }

            // emplace has toggle semantics on an existing element, so drop
            // a closed dialog first: the new offer must rebuild its pages.
            UI::get().remove(UIElement::QUESTDIALOG);
            UI::get().emplace<UIQuestDialog>(offer, npc_id);
            UI::get().enable();
            if (auto dialog = UI::get().get_element<UIQuestDialog>())
            {
                dialog->makeactive();
            }
        }
    }
}
