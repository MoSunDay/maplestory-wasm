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
#include "../UIDragElement.h"

#include "../../Character/CharStats.h"
#include "../../Graphics/Text.h"

#include <array>

namespace jrc
{
    class UIStatsinfo : public UIDragElement<PosSTATS>
    {
    public:
        static constexpr Type TYPE = STATSINFO;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = true;

        UIStatsinfo(const CharStats& stats);

        void draw(float alpha) const override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

        void update_all_stats();
        void update_stat(Maplestat::Id stat);

    protected:
        Button::State button_pressed(uint16_t buttonid) override;

    private:
        // Total label slots: the post-Chaos panel carries eight extra
        // detail rows the v83 bitmap has no sockets for.
        static const size_t NUMLABELS = 27;
        static const size_t NUMNORMAL = 12;
        static const size_t NUMDETAIL = 7;
        enum StatLabel
        {
            // Normal
            NAME, JOB, GUILD, FAME, DAMAGE,
            HP, MP, AP, STR, DEX, INT, LUK,
            // Detailed
            ATTACK, WDEF, MDEF, ACCURACY,
            AVOID, SPEED, JUMP,
            // Detailed (post-Chaos only)
            CRIT, MINCRIT, MAXCRIT,
            BDM, IGNOREDEF, RESIST, STANCE, HONOR
        };

        void update_ap();
        void update_simple(StatLabel label, Maplestat::Id stat);
        void update_basevstotal(StatLabel label, Maplestat::Id bstat, Equipstat::Id tstat);
        void update_buffed(StatLabel label, Equipstat::Id stat);
        void send_apup(Maplestat::Id stat) const;

        enum Buttons
        {
            BT_HP,
            BT_MP,
            BT_STR,
            BT_DEX,
            BT_INT,
            BT_LUK,
            // v83 ships a single toggle for the detail panel.
            BT_DETAIL,
            // post-Chaos has one button per panel state.
            BT_DETAILOPEN,
            BT_DETAILCLOSE,
            NUM_BUTTONS
        };

        // Loads the sprites, buttons and row offsets of the loaded UI.nx
        // generation (see UIStatsInfoNodes.h).
        void initialize_postchaos();
        void initialize_prechaos();

        const CharStats& stats;

        enum Ability
        {
            RARE,
            EPIC,
            UNIQUE,
            LEGENDARY,
            NONE,
            NUM_ABILITIES
        };

        std::array<Texture, NUM_ABILITIES> abilities;

        std::vector<Texture> textures_detail;
        bool showdetail;

        bool hasap;

        // Generation geometry picked by initialize_postchaos/prechaos:
        // the number of label rows the bitmap actually carries, the x
        // offset of the detail panel and the extra shift its rows need
        // (the v83 offsets already include the panel shift).
        size_t numlabels;
        int16_t detailx;
        int16_t detailshift;

        Text statlabels[NUMLABELS];
        Point<int16_t> statoffsets[NUMLABELS];
    };
}
