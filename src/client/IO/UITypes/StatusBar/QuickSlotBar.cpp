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
#include "QuickSlotBar.h"

#include "StatusBarPreChaos.h"

#include "../../../Data/SkillData.h"
#include "../../../Graphics/Geometry.h"

#include <cstddef>

namespace jrc
{
    namespace
    {
        void draw_slot(const Texture& texture, Point<int16_t> center, Point<int16_t> origin)
        {
            // Texture drawing applies the NX origin, so convert the wanted
            // top-left into the matching draw position.
            const Point<int16_t> half = texture.get_dimensions() / 2;
            const Point<int16_t> topleft = center - half;
            texture.draw(DrawArgument(origin + topleft + texture.get_origin()));
        }
    }

    namespace quickslot
    {
        void draw(const State& state, const Texture& panel,
            const Texture (&keys)[COUNT],
            const std::array<Text, COUNT>& cooldown_labels,
            Point<int16_t> origin)
        {
            panel.draw(origin);

            for (int16_t i = 0; i < COUNT; ++i)
            {
                const Point<int16_t> center = statusbar::quickslot_key_center(i);
                const int32_t skill_id = state.skills[static_cast<size_t>(i)];
                if (skill_id > 0)
                {
                    draw_slot(
                        SkillData::get(skill_id).get_icon(SkillData::NORMAL),
                        center,
                        origin
                    );

                    const Text& cooldown = cooldown_labels[static_cast<size_t>(i)];
                    if (!cooldown.empty())
                    {
                        constexpr int16_t COVER_SIZE = 32;
                        const ColorBox cover(
                            COVER_SIZE, COVER_SIZE, Geometry::BLACK, 0.55f
                        );
                        cover.draw(DrawArgument(
                            origin + center
                                - Point<int16_t>(COVER_SIZE / 2, COVER_SIZE / 2)
                        ));

                        // Text::CENTER already centers horizontally on the
                        // draw position; only lift half a line for vertical
                        // centering inside the cover.
                        const Point<int16_t> label_size = cooldown.dimensions();
                        cooldown.draw(DrawArgument(
                            origin + center
                                - Point<int16_t>(0, label_size.y() / 2)
                        ));
                    }
                }
                else
                {
                    draw_slot(keys[i], center, origin);
                }
            }
        }
    }
}
