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
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
// Map markers and the npc list of the minimap. Split out of UIMiniMap.cpp
// to keep both files within the size limits.
#include "UIMiniMap.h"

#include "UIMiniMapNodes.h"

#include "../../Gameplay/MapleMap/Npc.h"

#include <algorithm>

namespace jrc
{
    namespace
    {
        void truncate_text(Text& label, int16_t max_width)
        {
            if (label.empty() || max_width <= 0 || label.width() <= max_width)
            {
                return;
            }

            std::string text = label.get_text();
            while (!text.empty() && label.width() > max_width)
            {
                text.pop_back();
                label.change_text(text + "..");
            }
        }
    }

    void UIMiniMap::draw_movable_markers(Point<int16_t> init_pos, float alpha) const
    {
        if (!has_map)
        {
            return;
        }

        Animation npc_marker(marker_node["npc"]);
        Point<int16_t> npc_offset = npc_marker.get_dimensions() / Point<int16_t>(2, 1);

        MapObjects* npcs = Stage::get().get_npcs().get_npcs();
        for (auto npc = npcs->begin(); npc != npcs->end(); ++npc)
        {
            Point<int16_t> npc_pos = npc->second->get_position();
            npc_marker.draw((npc_pos + center_offset) / scale - npc_offset + Point<int16_t>(map_draw_origin_x, map_draw_origin_y) + init_pos, alpha);
        }

        Animation other_marker(marker_node["another"]);
        Point<int16_t> other_offset = other_marker.get_dimensions() / Point<int16_t>(2, 1);

        MapObjects* chars = Stage::get().get_chars().get_chars();
        for (auto chr = chars->begin(); chr != chars->end(); ++chr)
        {
            Point<int16_t> chr_pos = chr->second->get_position();
            other_marker.draw((chr_pos + center_offset) / scale - other_offset + Point<int16_t>(map_draw_origin_x, map_draw_origin_y) + init_pos, alpha);
        }

        Point<int16_t> player_pos = Stage::get().get_player().get_position();
        Point<int16_t> player_offset = player_marker.get_dimensions() / Point<int16_t>(2, 1);
        player_marker.draw((player_pos + center_offset) / scale - player_offset + Point<int16_t>(map_draw_origin_x, map_draw_origin_y) + init_pos, alpha);
    }

    void UIMiniMap::update_static_markers()
    {
        static_marker_info.clear();

        if (!has_map)
        {
            return;
        }

        Animation portal_marker(marker_node["portal"]);
        Point<int16_t> marker_offset = portal_marker.get_dimensions() / Point<int16_t>(2, 1);

        for (nl::node portal : map_node["portal"])
        {
            int32_t portal_type = portal["pt"];
            if (portal_type == 2)
            {
                Point<int16_t> marker_pos =
                    (Point<int16_t>(portal["x"], portal["y"]) + center_offset) / scale -
                    marker_offset +
                    Point<int16_t>(map_draw_origin_x, map_draw_origin_y);
                static_marker_info.emplace_back(portal.name(), marker_pos);
            }
        }
    }

    void UIMiniMap::set_npclist_active(bool active)
    {
        // v83 ships no ListNpc chrome, so the list can never be shown.
        list_npc_enabled = active && minimap_nodes::has_npclist();

        if (!list_npc_enabled)
        {
            select_npclist(-1);
        }

        update_dimensions();
    }

    void UIMiniMap::update_npclist()
    {
        list_npc_sprites.clear();
        list_npc_names.clear();
        list_npc_full_names.clear();
        list_npc_list.clear();
        selected = -1;
        list_npc_offset = 0;

        if (!minimap_nodes::has_npclist())
        {
            list_npc_slider.setenabled(false);
            return;
        }

        MapObjects* npcs = Stage::get().get_npcs().get_npcs();

        for (auto npc = npcs->begin(); npc != npcs->end(); ++npc)
        {
            list_npc_list.emplace_back(npc->second.get());

            Npc* map_npc = static_cast<Npc*>(npc->second.get());
            std::string name = map_npc->get_name();
            std::string func = map_npc->get_func();

            if (!func.empty())
            {
                name += " (" + func + ")";
            }

            Text name_text(Text::A11M, Text::LEFT, Text::WHITE, name);
            truncate_text(name_text, LISTNPC_TEXT_WIDTH - (npcs->size() > 8 ? 0 : 20));

            list_npc_names.emplace_back(name_text);
            list_npc_full_names.emplace_back(name);
        }

        const Point<int16_t> list_pos(type == MAX ? max_dimensions.x() : normal_dimensions.x(), 0);
        int16_t c_stretch = 20;
        int16_t m_stretch = 102;

        if (list_npc_names.size() > 8)
        {
            list_npc_slider = Slider(
                0,
                Range<int16_t>(23, 11 + LISTNPC_ITEM_HEIGHT * 8),
                list_pos.x() + LISTNPC_ITEM_WIDTH + 1,
                8,
                static_cast<int16_t>(list_npc_names.size()),
                [&](bool upwards)
                {
                    int16_t shift = upwards ? -1 : 1;
                    bool above = list_npc_offset + shift >= 0;
                    bool below = list_npc_offset + 8 + shift <= static_cast<int16_t>(list_npc_names.size());

                    if (above && below)
                    {
                        list_npc_offset += shift;
                    }
                }
            );

            c_stretch += 12;
        }
        else
        {
            list_npc_slider.setenabled(false);
            m_stretch = std::max<int16_t>(0, LISTNPC_ITEM_HEIGHT * static_cast<int16_t>(list_npc_names.size()) - 34);
            c_stretch -= 17;
        }

        list_npc_sprites.emplace_back(list_npc_node["c"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X, M_START), Point<int16_t>(c_stretch, m_stretch)));
        list_npc_sprites.emplace_back(list_npc_node["w"], DrawArgument(list_pos + Point<int16_t>(0, M_START), Point<int16_t>(0, m_stretch)));
        list_npc_sprites.emplace_back(list_npc_node["e"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X + c_stretch, M_START), Point<int16_t>(0, m_stretch)));
        list_npc_sprites.emplace_back(list_npc_node["n"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X, 0), Point<int16_t>(c_stretch, 0)));
        list_npc_sprites.emplace_back(list_npc_node["s"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X, M_START + m_stretch), Point<int16_t>(c_stretch, 0)));
        list_npc_sprites.emplace_back(list_npc_node["nw"], DrawArgument(list_pos + Point<int16_t>(0, 0)));
        list_npc_sprites.emplace_back(list_npc_node["ne"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X + c_stretch, 0)));
        list_npc_sprites.emplace_back(list_npc_node["sw"], DrawArgument(list_pos + Point<int16_t>(0, M_START + m_stretch)));
        list_npc_sprites.emplace_back(list_npc_node["se"], DrawArgument(list_pos + Point<int16_t>(CENTER_START_X + c_stretch, M_START + m_stretch)));

        list_npc_dimensions = Point<int16_t>(CENTER_START_X * 2 + c_stretch, M_START + m_stretch + 30);
        update_dimensions();
    }

    void UIMiniMap::draw_npclist(Point<int16_t> minimap_dims, float alpha) const
    {
        Animation npc_marker(marker_node["npc"]);

        for (const Sprite& sprite : list_npc_sprites)
        {
            sprite.draw(position, alpha);
        }

        Point<int16_t> list_pos = position + Point<int16_t>(minimap_dims.x() + 10, 23);

        for (int16_t i = 0; i + list_npc_offset < static_cast<int16_t>(list_npc_list.size()) && i < 8; ++i)
        {
            if (selected - list_npc_offset == i)
            {
                ColorBox highlight(
                    LISTNPC_ITEM_WIDTH - (list_npc_slider.isenabled() ? 0 : 30),
                    LISTNPC_ITEM_HEIGHT,
                    Geometry::WHITE,
                    0.35f
                );
                highlight.draw(DrawArgument(list_pos));
            }

            npc_marker.draw(DrawArgument(list_pos + Point<int16_t>(0, 2)), alpha);
            list_npc_names[list_npc_offset + i].draw(list_pos + Point<int16_t>(14, -2));
            list_pos.shift_y(LISTNPC_ITEM_HEIGHT);
        }

        if (list_npc_slider.isenabled())
        {
            list_npc_slider.draw(position);
        }

        if (selected >= 0 && selected < static_cast<int16_t>(list_npc_list.size()))
        {
            Point<int16_t> npc_pos =
                (list_npc_list[selected]->get_position() + center_offset) / scale +
                Point<int16_t>(map_draw_origin_x, map_draw_origin_y - selected_marker.get_dimensions().y() + (type == MAX ? minimap_nodes::max_adjust() : 0));

            selected_marker.draw(position + npc_pos, 0.5f);
        }
    }

    void UIMiniMap::select_npclist(int16_t choice)
    {
        if (selected >= 0 && selected < static_cast<int16_t>(list_npc_names.size()))
        {
            list_npc_names[selected].change_color(Text::WHITE);
        }

        if (choice < 0 || choice >= static_cast<int16_t>(list_npc_names.size()))
        {
            selected = -1;
            return;
        }

        selected = choice;
        list_npc_names[selected].change_color(Text::BLACK);
    }
}
