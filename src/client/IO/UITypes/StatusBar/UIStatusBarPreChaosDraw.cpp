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
// Status bar drawing helpers: the v83 StatusBar.img gauges/labels.
#include "../UIStatusBar.h"

namespace jrc
{
    void UIStatusbar::draw_prechaos() const
    {
        const Point<int16_t> gauge_pos = position + statusbar::gauge_anchor();

        // gauge/bar carries the labels plus the 100% gradient fills. The
        // unfilled rest of each slot is dimmed to the empty-slot look of
        // gauge/graduation, which is drawn on top (frame, ticks, base).
        gauge_fill.draw(gauge_pos);

        auto draw_cover = [this, gauge_pos](statusbar::Gauge kind, float percent) {
            const Rectangle<int16_t> fill = statusbar::gauge_fill(kind);
            const int16_t width = fill.width();
            const int16_t filled = static_cast<int16_t>(percent * width + 0.5f);
            const int16_t empty = static_cast<int16_t>(width - filled);
            if (empty <= 0)
            {
                return;
            }

            const int16_t index = static_cast<int16_t>(kind);
            // The covers start as 1x1 boxes; without a height refresh they
            // stay 1px tall and every gauge reads as visually full.
            gauge_covers[index].setwidth(empty);
            gauge_covers[index].setheight(fill.height());
            gauge_covers[index].draw(gauge_pos + Point<int16_t>(
                static_cast<int16_t>(fill.l() + filled), fill.t()
            ));
        };

        draw_cover(statusbar::Gauge::HP, gethppercent());
        draw_cover(statusbar::Gauge::MP, getmppercent());
        draw_cover(statusbar::Gauge::EXP, getexppercent());

        gauge_graduation.draw(gauge_pos);

        ensure_stat_labels(true);

        statset.draw(
            hp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::HP),
                statusbar::gauge_number_y()
            )
        );
        statset.draw(
            mp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::MP),
                statusbar::gauge_number_y()
            )
        );
        statset.draw(
            exp_label,
            gauge_pos + Point<int16_t>(
                statusbar::gauge_number_right(statusbar::Gauge::EXP),
                statusbar::gauge_number_y()
            )
        );

        levelset.draw(level_label, position + statusbar::level_pos());
        namelabel.draw(position + statusbar::name_pos());
        joblabel.draw(position + statusbar::job_pos());

    }
}
