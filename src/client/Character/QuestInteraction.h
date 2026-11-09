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
#pragma once

#include <cstdint>

namespace jrc
{
    // Client-side fallback for quest NPCs whose dialogue never arrives.
    //
    // The v83-linked server only opens NPC dialogs for scripted NPCs; plain
    // quest NPCs answer TalkToNPC with silence. This module watches those
    // requests: arm_npc_talk records the template id when the client sends
    // TalkToNPCPacket, the NpcDialogue/OpenNpcShop handlers cancel the
    // watch when the server really did open something, and the per-frame
    // update() decides a local quest offer dialog once the wait timed out.
    //
    // Free functions with function-local static state (no classes); the UI
    // loop is single threaded so one slot is enough.
    namespace quest_interaction
    {
        // Called when the player initiates an NPC talk (same moment the
        // TalkToNPCPacket is dispatched).
        void arm_npc_talk(int32_t npc_id);

        // Called when the server really opened an npc dialog or shop;
        // cancels the pending fallback.
        void cancel_npc_talk();

        // Per-frame tick: once an armed talk is older than NPC_TALK_TIMEOUT
        // and no server-driven dialog is on screen, the client resolves a
        // quest offer itself and opens the UIQuestDialog. Does nothing (and
        // clears the slot) when there is no offer.
        void update();
    }
}
