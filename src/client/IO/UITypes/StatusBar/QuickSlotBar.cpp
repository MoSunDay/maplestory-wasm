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

#include "../../../Data/ItemData.h"
#include "../../../Data/SkillData.h"
#include "../../../Graphics/Geometry.h"

#include <cstddef>

namespace jrc
{
    namespace
    {
        void draw_slot(const Texture& texture, Point<int16_t> center, Point<int16_t> origin, Color tint)
        {
            // Texture drawing applies the NX origin, so convert the wanted
            // top-left into the matching draw position.
            const Point<int16_t> half = texture.get_dimensions() / 2;
            const Point<int16_t> topleft = center - half;
            texture.draw(DrawArgument(origin + topleft + texture.get_origin(), tint));
        }
    }

    namespace quickslot
    {
        void draw(const Layout& layout, const State& state,
            const Texture& panel,
            const Texture (&keys)[COUNT],
            const std::array<Text, COUNT>& cooldown_labels,
            Point<int16_t> origin)
        {
            panel.draw(quickslot::texture_draw_position(origin, panel.get_origin()));

            for (int16_t i = 0; i < COUNT; ++i)
            {
                const size_t slot = static_cast<size_t>(i);
                const Point<int16_t> center = key_center(layout, i);
                const int32_t skill_id = state.skills[slot];
                const int32_t item_id = state.items[slot];
                const bool empty = skill_id <= 0 && item_id <= 0;

                if (skill_id > 0)
                {
                    draw_slot(
                        SkillData::get(skill_id).get_icon(SkillData::NORMAL),
                        center,
                        origin,
                        wide_attack_icon_tint(skill_id)
                    );

                    const Text& cooldown = cooldown_labels[slot];
                    if (!cooldown.empty())
                    {
                        const int16_t cover_size = layout.cover_size;
                        const ColorBox cover(
                            cover_size, cover_size, Geometry::BLACK, 0.55f
                        );
                        cover.draw(DrawArgument(
                            origin + center
                                - Point<int16_t>(cover_size / 2, cover_size / 2)
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
                else if (item_id > 0)
                {
                    draw_slot(
                        ItemData::get(item_id).get_icon(false),
                        center,
                        origin,
                        { 1.0f, 1.0f, 1.0f, 1.0f }
                    );
                }

                if (layout.labels_outside_slots)
                {
                    // The post-Chaos bitmap has dedicated header/base bands;
                    // draw every keycap there so the key name remains visible
                    // after the slot is bound to a skill or item.
                    const Point<int16_t> label_center(
                        layout.column_center[static_cast<size_t>(i % 4)],
                        layout.key_label_y[static_cast<size_t>(i / 4)]
                    );
                    draw_slot(keys[slot], label_center, origin,
                        { 1.0f, 1.0f, 1.0f, 1.0f });
                }
                else if (empty)
                {
                    draw_slot(keys[slot], center, origin,
                        { 1.0f, 1.0f, 1.0f, 1.0f });
                }
            }
        }
    }
}
