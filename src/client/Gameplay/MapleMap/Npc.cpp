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
//#pragma once
#include "Npc.h"

#include "../../Character/QuestCheck.h"
#include "../../Character/QuestInteraction.h"
#include "../../Character/QuestLog.h"
#include "../Stage.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>


namespace
{
    // Top-level npc nodes which are not animation stances:
    // 'shop' is the shop portrait, 'condition*' map quest state to stances.
    bool is_special_node(const std::string& name)
    {
        return name == "info" || name == "shop"
            || name.rfind("condition", 0) == 0;
    }

    // Checks if the node has at least one direct bitmap child, which
    // mirrors how 'Animation' collects its numbered frames.
    bool has_bitmap_frame(const nl::node& node)
    {
        for (auto child : node)
        {
            if (child.data_type() == nl::node::type::bitmap)
            {
                return true;
            }
        }

        return false;
    }

    // Numeric suffix of a condition node name ("condition12" -> 12); 0
    // when the name carries no number. The client evaluates the
    // conditions in this order and uses the first match.
    int32_t condition_order(const std::string& name)
    {
        const size_t at = name.rfind("condition");
        const std::string suffix =
            at == 0 ? name.substr(std::strlen("condition")) : std::string();
        const int32_t value = std::atoi(suffix.c_str());
        return value > 0 ? value : 0;
    }

    // Current state of one quest for the condition gates, using the
    // value range the Npc.nx condition tables encode: 1 = not started,
    // 2 = started (either local active bucket), 3 = completed.
    int32_t quest_gate_state(int32_t qid, const jrc::Questlog& quests)
    {
        if (quests.list_completed().count(
                static_cast<int16_t>(qid)) > 0)
        {
            return 3;
        }
        if (quests.list_started().count(static_cast<int16_t>(qid)) > 0
            || quests.list_in_progress().count(
                static_cast<int16_t>(qid)) > 0)
        {
            return 2;
        }
        return 1;
    }

    // Every quest-state gate of the condition must hold.
    bool condition_gates_met(const jrc::NpcCondition& condition,
        const jrc::Questlog& quests)
    {
        for (const jrc::NpcConditionGate& gate : condition.gates)
        {
            if (quest_gate_state(gate.quest_id, quests) != gate.state)
            {
                return false;
            }
        }
        return true;
    }

    // Picks the stance the npc should display out of its current base
    // stance and the condition stances: the first condition (numeric
    // name order) whose quest-state gates all hold replaces the base
    // stance; the returned key is "<condition>/<stance>" when that
    // condition ships art for the base stance and empty otherwise. Pure,
    // so the selection stays testable without an npc instance.
    std::string stance_for_quest_state(const std::string& base_stance,
        const std::vector<jrc::NpcCondition>& conditions,
        const jrc::Questlog& quests)
    {
        for (const jrc::NpcCondition& condition : conditions)
        {
            if (!condition_gates_met(condition, quests))
            {
                continue;
            }

            const auto art = std::find(
                condition.stances.begin(), condition.stances.end(),
                base_stance);
            if (art != condition.stances.end())
            {
                return condition.name + "/" + base_stance;
            }
            // The condition matches but carries no art for this stance:
            // keep the base stance rather than drawing nothing.
            return std::string();
        }
        return std::string();
    }
}


namespace jrc
{
    Npc::Npc(int32_t id,
             int32_t o,
             bool fl,
             uint16_t f,
             bool cnt,
             Point<int16_t> position)
        : MapObject(o)
    {
        std::string strid = std::to_string(id);
        strid.insert(0, 7 - strid.size(), '0');
        strid.append(".img");

        nl::node src    = nl::nx::npc[strid];
        nl::node strsrc = nl::nx::string["Npc.img"][std::to_string(id)];

        std::string link = src["info"]["link"];
        if (link.size() > 0)
        {
            link.append(".img");
            src = nl::nx::npc[link];
        }

        nl::node info = src["info"];

        hidename  = info["hideName"].get_bool();
        mouseonly = info["talkMouseOnly"].get_bool();
        scripted  = info["script"].size() > 0 || info["shop"].get_bool();

        for (const auto& npcnode : src)
        {
            const std::string state = npcnode.name();
            if (is_special_node(state))
            {
                // 'shop'/'condition*' subtrees are not stances: parsing
                // them as such made the npc vanish once the random idle
                // switch landed on one. Conditions become their own
                // table; the shop portrait stays unloaded like 'info'.
                if (state.rfind("condition", 0) == 0)
                {
                    load_condition(npcnode);
                }
                continue;
            }

            animations[state] = npcnode;
            states.push_back(state);

            for (auto speaknode : npcnode["speak"])
            {
                lines[state].push_back(strsrc[speaknode.get_string()]);
            }
        }

        // The client evaluates conditions by numeric name order; the nx
        // child order happens to match, but sorting keeps that
        // independent of the file layout.
        std::sort(conditions.begin(), conditions.end(),
            [](const NpcCondition& a, const NpcCondition& b)
            {
                return condition_order(a.name)
                    < condition_order(b.name);
            });

        name = strsrc["name"].get_string();
        func = strsrc["func"].get_string();

        namelabel = { Text::A13B, Text::CENTER, Text::YELLOW, Text::NAMETAG, name };
        funclabel = { Text::A13B, Text::CENTER, Text::YELLOW, Text::NAMETAG, func };

        npcid   = id;
        flip    = !fl;
        control = cnt;
        stance  = "stand";
        drawn_stance = stance;

        phobj.fhid = f;
        set_position(position);
    }

    // NX layout (verified against assets/Npc.nx and the linked server's
    // Npc.wz XML): a 'conditionN' node mixes quest-state gates with
    // replacement stance art,
    //   <imgdir name="condition1">
    //     <int name="21300" value="3"/>          quest gate (state)
    //     <int name="hide" value="1"/>           hide the npc instead
    //     <imgdir name="stand">...</imgdir>      replacement stance art
    //   </imgdir>
    // The state values 1/2/3 mean not started/started/completed (chains
    // like 1201001.gate {21300=3, 21302=2} are only consistent in that
    // reading: an earlier quest cannot be 'started' while a later one is
    // already completed).
    void Npc::load_condition(const nl::node& condition)
    {
        NpcCondition built;
        built.name = condition.name();
        for (const nl::node& child : condition)
        {
            if (child.data_type() == nl::node::type::integer)
            {
                if (child.name() == "hide")
                {
                    built.hidden = child.get_integer() == 1;
                }
                else
                {
                    built.gates.push_back(NpcConditionGate{
                        std::atoi(child.name().c_str()),
                        static_cast<int32_t>(child.get_integer())
                    });
                }
                continue;
            }

            // Art subtrees only; gates never carry frames.
            if (has_bitmap_frame(child))
            {
                built.stances.push_back(child.name());
                animations[built.name + "/" + child.name()] = child;
            }
        }
        conditions.push_back(std::move(built));
    }

    // Quest-state-driven stance selection: while a condition's gates
    // hold, its art replaces the base stance; 'hide' conditions remove
    // the npc from view until the quest state moves on.
    void Npc::resolve_condition_stance()
    {
        const std::string previous = drawn_stance;
        bool hidden = false;

        std::string override_stance;
        if (!conditions.empty())
        {
            const Questlog& quests =
                Stage::get().get_player().get_quests();
            override_stance =
                stance_for_quest_state(stance, conditions, quests);
            for (const NpcCondition& condition : conditions)
            {
                if (condition_gates_met(condition, quests))
                {
                    hidden = condition.hidden;
                    break;
                }
            }
        }

        condition_hidden = hidden;
        drawn_stance = override_stance.empty() ? stance : override_stance;
        if (drawn_stance != previous && animations.count(drawn_stance))
        {
            // Entering another animation restarts it from frame zero,
            // like set_stance does for the base stances.
            animations.at(drawn_stance).reset();
        }
    }

    void Npc::draw(double viewx, double viewy, float alpha) const
    {
        if (condition_hidden)
        {
            return;
        }

        Point<int16_t> absp = phobj.get_absolute(viewx, viewy, alpha);
        if (animations.count(drawn_stance))
        {
            animations.at(drawn_stance)
                .draw(DrawArgument(absp, flip), alpha);
        }

        if (!hidename)
        {
            namelabel.draw(absp);
            funclabel.draw(absp + Point<int16_t>(0, 18));
        }
    }

    int8_t Npc::update(const Physics& physics)
    {
        if (!active)
        {
            return phobj.fhlayer;
        }

        physics.move_object(phobj);

        // Quest conditions are re-checked every frame so the stance (and
        // the hidden flag) follows the live quest log; the walk is a few
        // map lookups per condition.
        resolve_condition_stance();

        if (animations.count(drawn_stance))
        {
            bool aniend = animations.at(drawn_stance).update();
            // Random idle switching only ranges over the base stances;
            // a condition override keeps its art until the state moves.
            if (aniend && states.size() > 0
                && drawn_stance == stance)
            {
                size_t next_stance = random.next_int(states.size());
                std::string new_stance = states[next_stance];
                set_stance(new_stance);
                resolve_condition_stance();
            }
        }

        return phobj.fhlayer;
    }

    void Npc::set_stance(const std::string& st)
    {
        if (stance != st)
        {
            stance = st;

            auto iter = animations.find(stance);
            if (iter == animations.end())
            {
                return;
            }

            iter->second.reset();
        }
    }

    bool Npc::isscripted() const
    {
        return scripted;
    }

    bool Npc::accepts_keyboard_talk() const
    {
        return active && !mouseonly;
    }

    bool Npc::inrange(Point<int16_t> cursorpos, Point<int16_t> viewpos) const
    {
        if (!active || condition_hidden)
        {
            return false;
        }

        Point<int16_t> absp = get_position() + viewpos;
        Point<int16_t> dim  =
            animations.count(stance) ?
                animations.at(stance).get_dimensions() :
                Point<int16_t>();

        return Rectangle<int16_t>(
            absp.x() - dim.x() / 2,
            absp.x() + dim.x() / 2,
            absp.y() - dim.y(),
            absp.y()
        ).contains(cursorpos);
    }

    bool npc_has_quest_marker(int32_t npc_id, const Questlog& quests,
        const QuestCheckContext& context)
    {
        QuestOffer offer{};
        return quest_offer_at(npc_id, quests, context, offer);
    }

    bool npc_has_quest_marker_now(int32_t npc_id)
    {
        Player& player = Stage::get().get_player();
        return npc_has_quest_marker(
            npc_id,
            player.get_quests(),
            quest_interaction::make_check_context()
        );
    }
}
