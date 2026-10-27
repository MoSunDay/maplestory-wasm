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
// Status bar drawing helper: the post-Chaos StatusBar2.img full-width strip
// and its left content column (level block, gauges, labels).
#include "../UIStatusBar.h"

namespace jrc
{
    void UIStatusbar::draw_postchaos() const
    {
        // Full-width bottom strip: only the flat left part of the bitmap
        // is drawn (its right end bakes a quickslot-cell grid that overlaps
        // the right control cluster), and it stretches so the bar covers
        // the bottom 100% of the screen at every resolution.
        const Point<int16_t> left = statusbar::postchaos_left_anchor();
        postchaos_backgrnd.draw(
            DrawArgument(
                left,
                Point<int16_t>(
                    Constants::viewwidth(), statusbar::CHAOS_BAR_HEIGHT
                )
            ),
            Rectangle<int16_t>(
                0, statusbar::CHAOS_BACKGRND_FLAT_WIDTH,
                0, statusbar::CHAOS_BAR_HEIGHT
            )
        );

        // Left content column: level block, gauge frame, notice icon.
        postchaos_lv_backgrnd.draw(left);
        postchaos_lv_cover.draw(left);
        postchaos_gauge_frame.draw(left);
        postchaos_notice.draw(left);

        expbar.draw(left + Point<int16_t>(-261, -15));
        hpbar.draw(left + Point<int16_t>(-261, -31));
        mpbar.draw(left + Point<int16_t>(-90, -31));

        ensure_stat_labels(statusbar::prechaos());

        statset.draw(exp_label, left + Point<int16_t>(47, -13));
        statset.draw(hp_label, left + Point<int16_t>(-124, -29));
        statset.draw(mp_label, left + Point<int16_t>(47, -29));
        levelset.draw(level_label, left + Point<int16_t>(-480, -24));

        joblabel.draw(left + Point<int16_t>(-435, -21));
        namelabel.draw(left + Point<int16_t>(-435, -36));
    }
}
