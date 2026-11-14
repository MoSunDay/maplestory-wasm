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
#include "MapObject.h"

#include "../Physics/PhysicsObject.h"

#include "../../Graphics/Animation.h"
#include "../../Graphics/Text.h"
#include "../../Util/Randomizer.h"

#include "nlnx/node.hpp"

#include <map>
#include <string>
#include <vector>

namespace jrc
{
    class Questlog;
    struct QuestCheckContext;

    // One quest-state gate of a 'conditionN' npc node: the quest must
    // sit in exactly this state for the condition to apply.
    struct NpcConditionGate
    {
        int32_t quest_id = 0;
        // Values observed in Npc.nx condition tables (see Npc.cpp):
        // 1 = not started, 2 = started (in progress), 3 = completed.
        int32_t state = 0;
    };

    // A 'conditionN' npc node: quest-state gates plus replacement stance
    // art the npc shows while the gates hold (some conditions carry
    // 'hide' = 1 instead, removing the npc from view entirely).
    struct NpcCondition
    {
        std::string name;                    // "condition1", ...
        std::vector<NpcConditionGate> gates; // quest state requirements
        bool hidden = false;                 // 'hide' == 1
        std::vector<std::string> stances;    // art subtree names ("stand")
    };

    // Asset-gate seam for the head-top quest balloon: this data set's
    // Effect.nx ships no Quest.img, so no marker is ever drawn. The pure
    // query below keeps the availability decision (QuestCheck::
    // quest_offer_at over the character's live log) in one place for the
    // future balloon art.
    bool npc_has_quest_marker(int32_t npc_id, const Questlog& quests,
        const QuestCheckContext& context);

    // Live-state variant of the above: builds the check context through
    // quest_interaction::make_check_context (the same builder the local
    // quest-offer fallback dialog uses).
    bool npc_has_quest_marker_now(int32_t npc_id);

    // Represents a npc on the current map.
    // Implements the 'Mapobject' interface to be used in a 'Mapobjects' template.
    class Npc : public MapObject
    {
    public:
        // Constructs an npc by combining data from game files with
        // data sent by the server.
        Npc(int32_t npcid, int32_t oid, bool mirrored, uint16_t fhid,
            bool control, Point<int16_t> position);

        // Draws the current animation and name/function tags.
        void draw(double viewx, double viewy, float alpha) const override;
        // Updates the current animation and physics.
        int8_t update(const Physics& physics) override;

        // Changes stance and resets animation.
        void set_stance(const std::string& stance);

        // Check wether this is a server-sided npc.
        bool isscripted() const;
        bool accepts_keyboard_talk() const;
        // Check if the npc is in range of the cursor.
        bool inrange(Point<int16_t> cursorpos, Point<int16_t> viewpos) const;

        const std::string& get_name() const
        {
            return name;
        }

        const std::string& get_func() const
        {
            return func;
        }

        // Template id from the map data (server dialogs key on the oid,
        // quest data keys on this id).
        int32_t get_npcid() const
        {
            return npcid;
        }

    private:
        // Loads one 'conditionN' child into the condition table and its
        // stance art into the animation map ("<condition>/<stance>" keys).
        void load_condition(const nl::node& condition);
        // Re-resolves which stance key the quest state selects; called
        // from update() so the npc follows the quest log live.
        void resolve_condition_stance();

        std::map<std::string, Animation> animations;
        std::map<std::string, std::vector<std::string>> lines;
        std::vector<std::string> states;
        // 'conditionN' nodes in numeric order; empty for plain npcs.
        std::vector<NpcCondition> conditions;
        // Stance key currently drawn: the base stance name, or a
        // "conditionN/<stance>" override.
        std::string drawn_stance;
        // The winning condition carries hide = 1: nothing is drawn.
        bool condition_hidden = false;
        std::string name;
        std::string func;
        bool hidename;
        bool scripted;
        bool mouseonly;

        int32_t npcid;
        bool flip;
        std::string stance;
        bool control;

        Randomizer random;
        Text namelabel;
        Text funclabel;
    };
}
