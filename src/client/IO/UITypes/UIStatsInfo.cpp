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
#include "UIStatsInfo.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Net/Packets/PlayerPackets.h"

#include "UIStatsInfoNodes.h"
#include "nlnx/nx.hpp"


namespace jrc
{
    UIStatsinfo::UIStatsinfo(const CharStats& st) :
        UIDragElement<PosSTATS>(statsinfo_nodes::window_position()), stats(st)
    {
        // The window geometry (nodes, buttons, row offsets) follows the
        // UI.nx generation detected at runtime; everything below the
        // layout is shared.
        if (statsinfo_nodes::post_chaos())
        {
            initialize_postchaos();
        }
        else
        {
            initialize_prechaos();
        }

        update_ap();

        for (size_t i = 0; i < NUMLABELS; ++i)
        {
            statlabels[i] = Text(Text::A11M, Text::LEFT, Text::LIGHTGREY);
        }

        update_all_stats();
        update_stat(Maplestat::JOB);
        update_stat(Maplestat::FAME);

        showdetail = false;
    }

    void UIStatsinfo::initialize_postchaos()
    {
        // post-Chaos splits the window into UIWindow4.img/Stat/main and
        // the detachable detail panel with 15 rows plus the ability
        // title art; one open/close button per panel state.
        nl::node src    = nl::nx::ui["UIWindow4.img"]["Stat"]["main"];
        nl::node detail = nl::nx::ui["UIWindow4.img"]["Stat"]["detail"];

        sprites.emplace_back(src["backgrnd"]);
        sprites.emplace_back(src["backgrnd2"]);
        sprites.emplace_back(src["backgrnd3"]);
        textures_detail.emplace_back(detail["backgrnd"]);
        textures_detail.emplace_back(detail["backgrnd2"]);
        textures_detail.emplace_back(detail["backgrnd3"]);

        abilities[RARE]      = detail["abilityTitle"]["rare"]["0"];
        abilities[EPIC]      = detail["abilityTitle"]["epic"]["0"];
        abilities[UNIQUE]    = detail["abilityTitle"]["unique"]["0"];
        abilities[LEGENDARY] = detail["abilityTitle"]["legendary"]["0"];
        abilities[NONE]      = detail["abilityTitle"]["normal"]["0"];

        buttons[BT_HP]  = std::make_unique<MapleButton>(src["BtHpUp"]);
        buttons[BT_MP]  = std::make_unique<MapleButton>(src["BtMpUp"]);
        buttons[BT_STR] = std::make_unique<MapleButton>(src["BtStrUp"]);
        buttons[BT_DEX] = std::make_unique<MapleButton>(src["BtDexUp"]);
        buttons[BT_LUK] = std::make_unique<MapleButton>(src["BtLukUp"]);
        buttons[BT_INT] = std::make_unique<MapleButton>(src["BtIntUp"]);

        buttons[BT_DETAILOPEN]  = std::make_unique<MapleButton>(src["BtDetailOpen"]);
        buttons[BT_DETAILCLOSE] = std::make_unique<MapleButton>(src["BtDetailClose"]);
        buttons[BT_DETAILCLOSE]->set_active(false);

        statoffsets[NAME]      = Point<int16_t>( 73, 27 );
        statoffsets[JOB]       = Point<int16_t>( 73, 45 );
        statoffsets[GUILD]     = Point<int16_t>( 73, 63 );
        statoffsets[FAME]      = Point<int16_t>( 73, 81 );
        statoffsets[DAMAGE]    = Point<int16_t>( 73, 99 );
        statoffsets[HP]        = Point<int16_t>( 73, 117);
        statoffsets[MP]        = Point<int16_t>( 73, 135);
        statoffsets[AP]        = Point<int16_t>( 70, 177);
        statoffsets[STR]       = Point<int16_t>( 73, 204);
        statoffsets[DEX]       = Point<int16_t>( 73, 222);
        statoffsets[INT]       = Point<int16_t>( 73, 240);
        statoffsets[LUK]       = Point<int16_t>( 73, 258);
        statoffsets[ATTACK]    = Point<int16_t>( 73, 37 );
        statoffsets[WDEF]      = Point<int16_t>( 73, 127);
        statoffsets[MDEF]      = Point<int16_t>( 73, 145);
        statoffsets[ACCURACY]  = Point<int16_t>( 73, 163);
        statoffsets[AVOID]     = Point<int16_t>( 73, 199);
        statoffsets[SPEED]     = Point<int16_t>( 73, 235);
        statoffsets[JUMP]      = Point<int16_t>(168, 235);
        statoffsets[CRIT]      = Point<int16_t>( 73, 55 );
        statoffsets[MINCRIT]   = Point<int16_t>( 73, 73 );
        statoffsets[MAXCRIT]   = Point<int16_t>(168, 73 );
        statoffsets[BDM]       = Point<int16_t>( 73, 91 );
        statoffsets[IGNOREDEF] = Point<int16_t>(168, 91 );
        statoffsets[RESIST]    = Point<int16_t>( 73, 109);
        statoffsets[STANCE]    = Point<int16_t>(168, 109);
        statoffsets[HONOR]     = Point<int16_t>( 73, 353);

        numlabels   = NUMLABELS;
        detailx     = 213;
        detailshift = 213;
        dimension   = Point<int16_t>(212, 318);
    }

    void UIStatsinfo::initialize_prechaos()
    {
        // v83 keeps the whole stat window in UIWindow.img/Stat: the main
        // panel (backgrnd, 175x347) and the detachable detail panel
        // (backgrnd2, 177x203).  The bitmap paints eight info rows at
        // y = 32 + 18 * n (the job row is taller), the secondary stat rows
        // STR/DEX/INT/LUK below the divider at y = 244 + 18 * n and ten
        // detail rows at y = 6 + 18 * n.  Values start at x = 56 on the
        // main panel and at x = 74 on the detail panel.
        nl::node src    = nl::nx::ui["UIWindow.img"]["Stat"];
        nl::node detail = src;

        sprites.emplace_back(src["backgrnd"]);
        textures_detail.emplace_back(detail["backgrnd2"]);

        // v83 only ships a single "AP up" sprite (BtApUp, 12x12); it is
        // reused for every raisable stat row.
        buttons[BT_HP]  = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 134));
        buttons[BT_MP]  = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 152));
        buttons[BT_STR] = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 246));
        buttons[BT_DEX] = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 264));
        buttons[BT_INT] = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 282));
        buttons[BT_LUK] = std::make_unique<MapleButton>(src["BtApUp"], Point<int16_t>(157, 300));

        buttons[BT_DETAIL] = std::make_unique<MapleButton>(src["BtDetail"], Point<int16_t>(64, 322));

        statoffsets[NAME]      = Point<int16_t>( 56,  34);
        statoffsets[JOB]       = Point<int16_t>( 56,  56);
        statoffsets[GUILD]     = Point<int16_t>( 56,  80);
        statoffsets[FAME]      = Point<int16_t>( 56,  98);
        statoffsets[DAMAGE]    = Point<int16_t>( 56, 116);
        statoffsets[HP]        = Point<int16_t>( 56, 134);
        statoffsets[MP]        = Point<int16_t>( 56, 152);
        statoffsets[AP]        = Point<int16_t>( 56, 170);
        statoffsets[STR]       = Point<int16_t>( 56, 246);
        statoffsets[DEX]       = Point<int16_t>( 56, 264);
        statoffsets[INT]       = Point<int16_t>( 56, 282);
        statoffsets[LUK]       = Point<int16_t>( 56, 300);
        statoffsets[ATTACK]    = Point<int16_t>(249,   8);
        statoffsets[WDEF]      = Point<int16_t>(249,  44);
        statoffsets[MDEF]      = Point<int16_t>(249,  62);
        statoffsets[ACCURACY]  = Point<int16_t>(249,  80);
        statoffsets[AVOID]     = Point<int16_t>(249,  98);
        statoffsets[SPEED]     = Point<int16_t>(249, 134);
        statoffsets[JUMP]      = Point<int16_t>(249, 152);

        numlabels   = NUMNORMAL + NUMDETAIL;
        detailx     = 175;
        detailshift = 0;
        dimension   = Point<int16_t>(175, 347);
    }

    void UIStatsinfo::draw(float alpha) const
    {
        UIElement::draw(alpha);

        if (showdetail)
        {
            Point<int16_t> detail_pos(position + Point<int16_t>(detailx, 0));
            for (auto& texture : textures_detail)
            {
                texture.draw(detail_pos);
            }
            // Ability titles only exist post-Chaos; the default Texture
            // the v83 path leaves here draws nothing.
            abilities[NONE].draw({ detail_pos });
        }

        size_t last = showdetail ? numlabels : NUMNORMAL;
        for (size_t i = 0; i < last; ++i)
        {
            Point<int16_t> labelpos = position + statoffsets[i];
            if (i >= NUMNORMAL)
            {
                // post-Chaos detail offsets are panel relative; the v83
                // table already includes the panel shift.
                labelpos.shift_x(detailshift);
            }

            statlabels[i].draw(labelpos);
        }
    }

    void UIStatsinfo::send_key(int32_t, bool pressed, bool escape)
    {
        if (pressed && escape)
        {
            deactivate();
        }
    }

    void UIStatsinfo::update_all_stats()
    {
        update_simple(AP, Maplestat::AP);
        if (hasap ^ (stats.get_stat(Maplestat::AP) > 0))
        {
            update_ap();
        }

        statlabels[NAME].change_text(stats.get_name());
        statlabels[GUILD].change_text("");

        statlabels[HP].change_text(std::to_string(stats.get_stat(Maplestat::HP)) + " / " + std::to_string(stats.get_total(Equipstat::HP)));
        statlabels[MP].change_text(std::to_string(stats.get_stat(Maplestat::MP)) + " / " + std::to_string(stats.get_total(Equipstat::MP)));

        update_basevstotal(STR, Maplestat::STR, Equipstat::STR);
        update_basevstotal(DEX, Maplestat::DEX, Equipstat::DEX);
        update_basevstotal(INT, Maplestat::INT, Equipstat::INT);
        update_basevstotal(LUK, Maplestat::LUK, Equipstat::LUK);

        statlabels[DAMAGE].change_text(std::to_string(stats.get_mindamage()) + " ~ " + std::to_string(stats.get_maxdamage()));
        if (stats.is_damage_buffed())
        {
            statlabels[DAMAGE].change_color(Text::RED);
        }
        else
        {
            statlabels[DAMAGE].change_color(Text::LIGHTGREY);
        }

        update_buffed(ATTACK, Equipstat::WATK);
        update_buffed(WDEF, Equipstat::WDEF);
        update_buffed(MDEF, Equipstat::MDEF);
        update_buffed(ACCURACY, Equipstat::ACC);
        update_buffed(AVOID, Equipstat::AVOID);

        statlabels[SPEED    ].change_text(std::to_string(stats.get_total(Equipstat::SPEED))                  + "%");
        statlabels[JUMP     ].change_text(std::to_string(stats.get_total(Equipstat::JUMP))                   + "%");

        // The rows below only have sockets on the post-Chaos detail
        // panel; on v83 they are filled but never drawn (see numlabels).
        statlabels[CRIT     ].change_text(std::to_string(static_cast<int32_t>(stats.get_critical()   * 100)) + "%");
        statlabels[MINCRIT  ].change_text(std::to_string(static_cast<int32_t>(stats.get_mincrit()    * 100)) + "%");
        statlabels[MAXCRIT  ].change_text(std::to_string(static_cast<int32_t>(stats.get_maxcrit()    * 100)) + "%");
        statlabels[BDM      ].change_text(std::to_string(static_cast<int32_t>(stats.get_bossdmg()    * 100)) + "%");
        statlabels[IGNOREDEF].change_text(std::to_string(static_cast<int32_t>(stats.get_ignoredef()  * 100)) + "%");
        statlabels[RESIST   ].change_text(std::to_string(static_cast<int32_t>(stats.get_resistance() * 100)) + "%");
        statlabels[STANCE   ].change_text(std::to_string(static_cast<int32_t>(stats.get_stance()     * 100)) + "%");
        statlabels[HONOR    ].change_text(std::to_string(stats.get_honor())                                       );
    }

    void UIStatsinfo::update_stat(Maplestat::Id stat)
    {
        switch (stat)
        {
        case Maplestat::JOB:
            statlabels[JOB].change_text(stats.get_jobname());
            break;
        case Maplestat::FAME:
            update_simple(FAME, Maplestat::FAME);
            break;
        default:
            break;
        }
    }

    Button::State UIStatsinfo::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_DETAILOPEN:
            // post-Chaos has one button per panel state.
            showdetail = true;
            buttons[BT_DETAILOPEN]->set_active(false);
            buttons[BT_DETAILCLOSE]->set_active(true);
            break;
        case BT_DETAILCLOSE:
            showdetail = false;
            buttons[BT_DETAILCLOSE]->set_active(false);
            buttons[BT_DETAILOPEN]->set_active(true);
            break;
        case BT_DETAIL:
            // v83 has a single toggle button for the detail panel.
            showdetail = !showdetail;
            break;
        case BT_HP:
            send_apup(Maplestat::HP);
            break;
        case BT_MP:
            send_apup(Maplestat::MP);
            break;
        case BT_STR:
            send_apup(Maplestat::STR);
            break;
        case BT_DEX:
            send_apup(Maplestat::DEX);
            break;
        case BT_INT:
            send_apup(Maplestat::INT);
            break;
        case BT_LUK:
            send_apup(Maplestat::LUK);
            break;
        default:
            break;
        }
        return Button::NORMAL;
    }

    void UIStatsinfo::send_apup(Maplestat::Id stat) const
    {
        SpendApPacket(stat).dispatch();
        UI::get().disable();
    }

    void UIStatsinfo::update_ap()
    {
        bool nowap = stats.get_stat(Maplestat::AP) > 0;
        Button::State newstate = nowap ? Button::NORMAL : Button::DISABLED;

        // Beginner AP assignment is controlled by the server: when starter AP
        // is manual, Cosmic exposes spendable AP through the normal stat pool.
        // The NX origins already place every button beside its matching row;
        // adding another offset moves both the sprite and its hit box away.
        buttons[BT_HP ]->set_state(newstate);
        buttons[BT_MP ]->set_state(newstate);
        buttons[BT_STR]->set_state(newstate);
        buttons[BT_DEX]->set_state(newstate);
        buttons[BT_LUK]->set_state(newstate);
        buttons[BT_INT]->set_state(newstate);

        hasap = nowap;
    }

    void UIStatsinfo::update_simple(StatLabel label, Maplestat::Id stat)
    {
        statlabels[label].change_text(std::to_string(stats.get_stat(stat)));
    }

    void UIStatsinfo::update_basevstotal(StatLabel label, Maplestat::Id bstat, Equipstat::Id tstat)
    {
        int32_t base  = stats.get_stat(bstat);
        int32_t total = stats.get_total(tstat);
        int32_t delta = total - base;

        std::string stattext = std::to_string(total);
        if (delta)
        {
            stattext += " (" + std::to_string(base);
            if (delta > 0)
            {
                stattext += " + " + std::to_string(delta);
            }
            else if (delta < 0)
            {
                stattext += " - " + std::to_string(-delta);
            }
            stattext += ")";
        }
        statlabels[label].change_text(stattext);
    }

    void UIStatsinfo::update_buffed(StatLabel label, Equipstat::Id stat)
    {
        int32_t total = stats.get_total(stat);
        int32_t delta = stats.get_buffdelta(stat);

        std::string stattext = std::to_string(total);
        if (delta)
        {
            stattext += " (" + std::to_string(total - delta);
            if (delta > 0)
            {
                stattext += " + " + std::to_string(delta);

                statlabels[label].change_color(Text::RED);
            }
            else if (delta < 0)
            {
                stattext += " - " + std::to_string(-delta);

                statlabels[label].change_color(Text::BLUE);
            }
            stattext += ")";
        }
        statlabels[label].change_text(stattext);
    }
}
