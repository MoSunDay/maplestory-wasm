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
#include "UIMiniMap.h"

#include "UIMiniMapNodes.h"

#include "UIWorldMap.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/ScrollingNotice.h"

#include "../../Gameplay/MapleMap/Npc.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    UIMiniMap::UIMiniMap(const CharStats& stats)
        : UIDragElement<PosMINIMAP>(Point<int16_t>(128, 20)),
          mapid(-1),
          type(Setting<MiniMapType>::get().load()),
          user_type(type),
          simple_mode(Setting<MiniMapSimpleMode>::get().load()),
          big_map(true),
          has_map(false),
          scale(1),
          combined_text(Text::A12M, Text::LEFT, Text::WHITE),
          region_text(Text::A12B, Text::LEFT, Text::WHITE),
          town_text(Text::A12B, Text::LEFT, Text::WHITE),
          list_npc_enabled(false),
          list_npc_dimensions(150, 170),
          list_npc_offset(0),
          selected(-1),
          stats(stats)
    {
        if (position.y() < ScrollingNotice::HEIGHT)
        {
            position.set_y(ScrollingNotice::HEIGHT);
        }

        minimap_node = minimap_nodes::src();
        list_npc_node = minimap_nodes::npclist_src();
        map_helper_node = nl::nx::map["MapHelper.img"];

        nl::node simple_helper = minimap_nodes::simple_helper_src();
        marker_node = Setting<MiniMapDefaultHelpers>::get().load() && simple_helper ?
            simple_helper :
            map_helper_node["minimap"];

        // v83 only ships BtMap; the size and npc-list buttons are
        // post-Chaos chrome and are not created when absent.
        if (minimap_nodes::has_size_buttons())
        {
            buttons[BT_MIN] = std::make_unique<MapleButton>(minimap_node["BtMin"], Point<int16_t>(195, -6));
            buttons[BT_MAX] = std::make_unique<MapleButton>(minimap_node["BtMax"], Point<int16_t>(209, -6));
            buttons[BT_SMALL] = std::make_unique<MapleButton>(minimap_node["BtSmall"], Point<int16_t>(223, -6));
            buttons[BT_BIG] = std::make_unique<MapleButton>(minimap_node["BtBig"], Point<int16_t>(223, -6));
        }

        buttons[BT_MAP] = std::make_unique<MapleButton>(
            minimap_node["BtMap"],
            minimap_nodes::post_chaos() ? Point<int16_t>(237, -6) : Point<int16_t>()
        );

        if (minimap_nodes::has_npc_button())
        {
            buttons[BT_NPC] = std::make_unique<MapleButton>(minimap_node["BtNpc"], Point<int16_t>(276, -6));
        }

        player_marker = Animation(marker_node["user"]);
        selected_marker = Animation(minimap_node["iconNpc"]);
        map_draw_origin_x = 0;
        map_draw_origin_y = 0;
        center_offset = {};

        update_buttons();
        toggle_buttons();
    }

    void UIMiniMap::draw(float alpha) const
    {
        if (type == MIN)
        {
            for (const Sprite& sprite : min_sprites)
            {
                sprite.draw(position, alpha);
            }

            combined_text.draw(position + (minimap_nodes::post_chaos() ? Point<int16_t>(7, -3) : minimap_nodes::min_text_pos()));
        }
        else if (type == NORMAL)
        {
            for (const Sprite& sprite : normal_sprites)
            {
                sprite.draw(position, alpha);
            }

            if (!minimap_nodes::post_chaos())
            {
                // v83 title band is 29px tall and hosts the map name next
                // to BtMap.
                combined_text.draw(position + Point<int16_t>(13, 9));
            }

            if (has_map)
            {
                Animation portal_marker(marker_node["portal"]);

                for (const auto& portal : static_marker_info)
                {
                    portal_marker.draw(position + portal.second, alpha);
                }

                draw_movable_markers(position, alpha);

                if (list_npc_enabled)
                {
                    draw_npclist(normal_dimensions, alpha);
                }
            }
        }
        else
        {
            for (const Sprite& sprite : max_sprites)
            {
                sprite.draw(position, alpha);
            }

            region_text.draw(position + (minimap_nodes::post_chaos() ? Point<int16_t>(48, 14) : minimap_nodes::max_region_pos()));
            town_text.draw(position + (minimap_nodes::post_chaos() ? Point<int16_t>(48, 28) : minimap_nodes::max_town_pos()));

            if (has_map)
            {
                Animation portal_marker(marker_node["portal"]);

                for (const auto& portal : static_marker_info)
                {
                    portal_marker.draw(position + portal.second + Point<int16_t>(0, minimap_nodes::max_adjust()), alpha);
                }

                draw_movable_markers(position + Point<int16_t>(0, minimap_nodes::max_adjust()), alpha);

                if (list_npc_enabled)
                {
                    draw_npclist(max_dimensions, alpha);
                }
            }
        }

        UIElement::draw(alpha);
    }

    void UIMiniMap::update()
    {
        int32_t current_mapid = Stage::get().get_mapid();

        if (current_mapid != mapid)
        {
            mapid = current_mapid;
            map_node = NxHelper::Map::get_map_node_name(mapid);

            nl::node town = map_node["info"]["town"];
            nl::node mini_map = map_node["miniMap"];
            has_map = static_cast<bool>(mini_map);

            if (!has_map)
            {
                type = MIN;
            }
            else if (town && town.get_bool())
            {
                type = MAX;
            }
            else
            {
                type = user_type;
            }

            int32_t mag = mini_map ? static_cast<int32_t>(mini_map["mag"]) : 0;
            scale = 1 << std::max(0, mag);
            center_offset = mini_map ? Point<int16_t>(mini_map["centerX"], mini_map["centerY"]) : Point<int16_t>();

            update_text();
            update_buttons();
            update_canvas();
            update_static_markers();
            toggle_buttons();
            update_npclist();
        }

        for (Sprite& sprite : min_sprites)
        {
            sprite.update();
        }

        for (Sprite& sprite : normal_sprites)
        {
            sprite.update();
        }

        for (Sprite& sprite : max_sprites)
        {
            sprite.update();
        }

        for (Sprite& sprite : list_npc_sprites)
        {
            sprite.update();
        }

        if (selected >= 0)
        {
            selected_marker.update();
        }

        player_marker.update();
        UIElement::update();
    }

    bool UIMiniMap::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        bool removed = UIDragElement::remove_cursor(clicked, cursorpos);
        bool slider_removed = list_npc_slider.remove_cursor(clicked);

        if (!is_in_range(cursorpos))
        {
            UI::get().clear_tooltip(Tooltip::MINIMAP);
        }

        return removed || slider_removed;
    }

    UIElement::CursorResult UIMiniMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            return UIDragElement::send_cursor(clicked, cursorpos);
        }

        Point<int16_t> relative = cursorpos - position;

        if (list_npc_slider.isenabled())
        {
            Cursor::State slider_state = list_npc_slider.send_cursor(relative, clicked);
            if (slider_state != Cursor::IDLE)
            {
                return { slider_state, true };
            }
        }

        if (list_npc_enabled)
        {
            Point<int16_t> list_origin = Point<int16_t>(
                10 + (type == MAX ? max_dimensions : normal_dimensions).x(),
                23
            );
            Rectangle<int16_t> list_bounds(
                list_origin,
                list_origin + Point<int16_t>(LISTNPC_ITEM_WIDTH, LISTNPC_ITEM_HEIGHT * 8)
            );

            if (list_bounds.contains(relative))
            {
                int16_t list_index = list_npc_offset + (relative.y() - list_origin.y()) / LISTNPC_ITEM_HEIGHT;
                bool in_list = list_index >= 0 && list_index < static_cast<int16_t>(list_npc_names.size());

                if (clicked)
                {
                    select_npclist(in_list ? list_index : -1);
                }
                else if (in_list)
                {
                    UI::get().show_text(Tooltip::MINIMAP, list_npc_full_names[list_index]);
                }

                return { Cursor::IDLE, true };
            }
        }

        bool found = false;
        MapObjects* npcs = Stage::get().get_npcs().get_npcs();

        for (auto npc = npcs->begin(); npc != npcs->end(); ++npc)
        {
            Point<int16_t> npc_pos =
                (npc->second->get_position() + center_offset) / scale +
                Point<int16_t>(map_draw_origin_x, map_draw_origin_y);
            Rectangle<int16_t> marker_spot(npc_pos - Point<int16_t>(4, 8), npc_pos + Point<int16_t>(4, 8));

            if (type == MAX)
            {
                marker_spot.shift(Point<int16_t>(0, minimap_nodes::max_adjust()));
            }

            if (marker_spot.contains(relative))
            {
                found = true;

                Npc* map_npc = static_cast<Npc*>(npc->second.get());
                UI::get().show_map(
                    Tooltip::MINIMAP,
                    map_npc->get_name(),
                    map_npc->get_func(),
                    0,
                    false,
                    false
                );
                break;
            }
        }

        if (!found)
        {
            for (const auto& portal : static_marker_info)
            {
                Rectangle<int16_t> marker_spot(portal.second, portal.second + Point<int16_t>(8, 8));

                if (type == MAX)
                {
                    marker_spot.shift(Point<int16_t>(0, minimap_nodes::max_adjust()));
                }

                if (marker_spot.contains(relative))
                {
                    int32_t target_mapid = map_node["portal"][portal.first]["tm"];
                    std::string portal_category = NxHelper::Map::get_map_category(target_mapid);
                    std::string target_map_str = std::to_string(target_mapid);
                    std::string target_name = nl::nx::string["Map.img"][portal_category][target_map_str]["mapName"];

                    if (!target_name.empty())
                    {
                        UI::get().show_map(Tooltip::MINIMAP, target_name, "", target_mapid, false, true);
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            UI::get().clear_tooltip(Tooltip::MINIMAP);
        }

        return UIDragElement::send_cursor(clicked, cursorpos);
    }

    void UIMiniMap::send_scroll(double yoffset)
    {
        if (list_npc_enabled && list_npc_slider.isenabled())
        {
            list_npc_slider.send_scroll(yoffset);
        }
    }

    void UIMiniMap::send_key(int32_t, bool pressed, bool)
    {
        if (!pressed || !has_map)
        {
            return;
        }

        type = type < MAX ? type + 1 : MIN;
        user_type = type;
        Setting<MiniMapType>::get().save(static_cast<uint8_t>(user_type));
        toggle_buttons();
    }

    Button::State UIMiniMap::button_pressed(uint16_t buttonid)
    {
        switch (buttonid)
        {
        case BT_MIN:
            if (type > MIN)
            {
                type -= 1;
                user_type = type;
                Setting<MiniMapType>::get().save(static_cast<uint8_t>(user_type));
                toggle_buttons();
            }
            return type == MIN ? Button::DISABLED : Button::NORMAL;
        case BT_MAX:
            if (has_map && type < MAX)
            {
                type += 1;
                user_type = type;
                Setting<MiniMapType>::get().save(static_cast<uint8_t>(user_type));
                toggle_buttons();
            }
            return type == MAX ? Button::DISABLED : Button::NORMAL;
        case BT_SMALL:
        case BT_BIG:
            big_map = !big_map;
            toggle_buttons();
            break;
        case BT_MAP:
            UI::get().emplace<UIWorldMap>();
            break;
        case BT_NPC:
            set_npclist_active(!list_npc_enabled);
            break;
        }

        return Button::NORMAL;
    }

    void UIMiniMap::update_buttons()
    {
        // v83 has no size buttons; their widths collapse to zero so the
        // collapsed-bar math below only accounts for BtMap.
        if (minimap_nodes::has_size_buttons())
        {
            bt_min_width = buttons[BT_MIN]->width() + 1;
            bt_max_width = buttons[BT_MAX]->width() + 1;
        }
        else
        {
            bt_min_width = 0;
            bt_max_width = 0;
        }

        bt_map_width = buttons[BT_MAP]->width() + 1;
        combined_text_width = combined_text.width();
    }

    void UIMiniMap::toggle_buttons()
    {
        if (!minimap_nodes::has_size_buttons())
        {
            toggle_buttons_v83();
            return;
        }

        int16_t bt_min_x = 0;

        if (type == MIN)
        {
            buttons[BT_MAP]->set_active(true);
            buttons[BT_MAX]->set_active(true);
            buttons[BT_MIN]->set_active(true);
            buttons[BT_NPC]->set_active(false);
            buttons[BT_SMALL]->set_active(false);
            buttons[BT_BIG]->set_active(false);

            buttons[BT_MIN]->set_state(Button::DISABLED);
            buttons[BT_MAX]->set_state(has_map ? Button::NORMAL : Button::DISABLED);

            bt_min_x = combined_text_width + 11;

            buttons[BT_MIN]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_min_width;
            buttons[BT_MAX]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_max_width;
            buttons[BT_MAP]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));

            min_dimensions = Point<int16_t>(bt_min_x + bt_map_width + 7, 20);

            update_dimensions();
            dragarea = dimension;
            set_npclist_active(false);
        }
        else
        {
            bool has_npcs = Stage::get().get_npcs().get_npcs()->size() > 0;

            buttons[BT_MAP]->set_active(true);
            buttons[BT_MAX]->set_active(true);
            buttons[BT_MIN]->set_active(true);
            buttons[BT_NPC]->set_active(has_npcs);

            buttons[BT_BIG]->set_active(!big_map);
            buttons[BT_SMALL]->set_active(big_map);
            buttons[BT_MIN]->set_state(Button::NORMAL);

            bt_min_x = middle_right_x - (
                bt_min_width +
                buttons[BT_SMALL]->width() + 1 +
                bt_max_width +
                bt_map_width +
                (has_npcs ? buttons[BT_NPC]->width() : 0)
            );

            buttons[BT_MIN]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_max_width;
            buttons[BT_MAX]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_max_width;
            buttons[BT_SMALL]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            buttons[BT_BIG]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_max_width;
            buttons[BT_MAP]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));
            bt_min_x += bt_map_width;
            buttons[BT_NPC]->set_position(Point<int16_t>(bt_min_x, BTN_MIN_Y));

            buttons[BT_MAX]->set_state(type == MAX ? Button::DISABLED : Button::NORMAL);
            set_npclist_active(list_npc_enabled && has_npcs);
            dragarea = Point<int16_t>(dimension.x(), 20);
        }
    }

    // v83 layout: BtMap is the only button, right-aligned inside the
    // collapsed bar or the title band of the expanded windows.
    void UIMiniMap::toggle_buttons_v83()
    {
        const int16_t map_width = bt_map_width - 1;

        if (type == MIN)
        {
            const int16_t bar_width = minimap_nodes::min_bar_width(combined_text_width, map_width);

            buttons[BT_MAP]->set_active(true);
            buttons[BT_MAP]->set_position(minimap_nodes::map_button_pos(bar_width, minimap_nodes::MIN_H, map_width));

            min_dimensions = Point<int16_t>(bar_width, minimap_nodes::MIN_H);

            update_dimensions();
            dragarea = dimension;
            set_npclist_active(false);
        }
        else
        {
            const int16_t window_width = (type == MAX ? max_dimensions : normal_dimensions).x();

            buttons[BT_MAP]->set_active(true);
            buttons[BT_MAP]->set_position(minimap_nodes::map_button_pos(window_width, minimap_nodes::NORMAL_TOP_H, map_width));

            dragarea = Point<int16_t>(dimension.x(), minimap_nodes::NORMAL_TOP_H);
        }
    }

    void UIMiniMap::update_text()
    {
        NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapid);
        combined_text.change_text(map_info.full_name);
        region_text.change_text(map_info.name);
        town_text.change_text(map_info.street_name);
    }

    void UIMiniMap::update_canvas()
    {
        min_sprites.clear();
        normal_sprites.clear();
        max_sprites.clear();

        nl::node min_src = minimap_node["Min"];
        nl::node normal_src = minimap_node["MinMap"];
        nl::node max_src = minimap_node["MaxMap"];
        nl::node middle_center = minimap_node["MaxMap"]["c"];

        map_sprite = Texture(map_node["miniMap"]["canvas"]);
        Point<int16_t> map_dimensions = map_sprite.get_dimensions();

        if (!minimap_nodes::post_chaos())
        {
            update_canvas_v83(map_dimensions);
            return;
        }

        int16_t mark_text_width = 48 + std::max(region_text.width(), town_text.width()) + 10;
        int16_t window_width = std::max<int16_t>(178, std::max<int16_t>(mark_text_width, map_dimensions.x() + 20));
        int16_t c_stretch = std::max<int16_t>(0, window_width - 128);
        int16_t ur_x_offset = CENTER_START_X + c_stretch;

        map_draw_origin_x = std::max<int16_t>(10, window_width / 2 - map_dimensions.x() / 2);

        int16_t m_stretch = 5;
        int16_t down_y_offset = 22;
        if (map_dimensions.y() > 20)
        {
            m_stretch = map_dimensions.y() - 17;
            down_y_offset = 17 + m_stretch;
            map_draw_origin_y = 20;
        }
        else
        {
            m_stretch = 5;
            down_y_offset = 17 + m_stretch;
            map_draw_origin_y = 10 + m_stretch - map_dimensions.y();
        }

        middle_right_x = ur_x_offset + 55;

        int16_t min_c_stretch = combined_text_width + 18 + bt_min_width + bt_max_width + bt_map_width - 128;

        min_sprites.emplace_back(min_src["c"], DrawArgument(Point<int16_t>(CENTER_START_X, 0), Point<int16_t>(min_c_stretch, 0)));
        min_sprites.emplace_back(min_src["w"], DrawArgument(Point<int16_t>(0, 0)));
        min_sprites.emplace_back(min_src["e"], DrawArgument(Point<int16_t>(min_c_stretch + CENTER_START_X, 0)));

        normal_sprites.emplace_back(middle_center, DrawArgument(Point<int16_t>(7, 10), Point<int16_t>(c_stretch + 114, m_stretch + 27)));
        if (has_map)
        {
            normal_sprites.emplace_back(map_node["miniMap"]["canvas"], DrawArgument(Point<int16_t>(map_draw_origin_x, map_draw_origin_y)));
        }

        normal_sprites.emplace_back(normal_src["w"], DrawArgument(Point<int16_t>(0, ML_MR_Y), Point<int16_t>(0, m_stretch)));
        normal_sprites.emplace_back(normal_src["e"], DrawArgument(Point<int16_t>(middle_right_x, ML_MR_Y), Point<int16_t>(0, m_stretch)));
        normal_sprites.emplace_back(normal_src["n"], DrawArgument(Point<int16_t>(CENTER_START_X, 0), Point<int16_t>(c_stretch, 0)));
        normal_sprites.emplace_back(normal_src["nw"], DrawArgument(Point<int16_t>(0, 0)));
        normal_sprites.emplace_back(normal_src["ne"], DrawArgument(Point<int16_t>(ur_x_offset, 0)));
        normal_sprites.emplace_back(normal_src["s"], DrawArgument(Point<int16_t>(CENTER_START_X, down_y_offset + 18), Point<int16_t>(c_stretch, 0)));
        normal_sprites.emplace_back(normal_src["sw"], DrawArgument(Point<int16_t>(0, down_y_offset)));
        normal_sprites.emplace_back(normal_src["se"], DrawArgument(Point<int16_t>(ur_x_offset, down_y_offset)));

        normal_dimensions = Point<int16_t>(ur_x_offset + 64, down_y_offset + 27);

        max_sprites.emplace_back(middle_center, DrawArgument(Point<int16_t>(7, 50), Point<int16_t>(c_stretch + 114, m_stretch + 27)));
        if (has_map)
        {
            max_sprites.emplace_back(map_node["miniMap"]["canvas"], DrawArgument(Point<int16_t>(map_draw_origin_x, map_draw_origin_y + MAX_ADJ)));
        }

        max_sprites.emplace_back(max_src["w"], DrawArgument(Point<int16_t>(0, ML_MR_Y + MAX_ADJ), Point<int16_t>(0, m_stretch)));
        max_sprites.emplace_back(max_src["e"], DrawArgument(Point<int16_t>(middle_right_x, ML_MR_Y + MAX_ADJ), Point<int16_t>(0, m_stretch)));
        max_sprites.emplace_back(max_src["n"], DrawArgument(Point<int16_t>(CENTER_START_X, 0), Point<int16_t>(c_stretch, 0)));
        max_sprites.emplace_back(max_src["nw"], DrawArgument(Point<int16_t>(0, 0)));
        max_sprites.emplace_back(max_src["ne"], DrawArgument(Point<int16_t>(ur_x_offset, 0)));
        max_sprites.emplace_back(max_src["s"], DrawArgument(Point<int16_t>(CENTER_START_X, down_y_offset + MAX_ADJ + 18), Point<int16_t>(c_stretch, 0)));
        max_sprites.emplace_back(max_src["sw"], DrawArgument(Point<int16_t>(0, down_y_offset + MAX_ADJ)));
        max_sprites.emplace_back(max_src["se"], DrawArgument(Point<int16_t>(ur_x_offset, down_y_offset + MAX_ADJ)));
        max_sprites.emplace_back(map_helper_node["mark"][map_node["info"]["mapMark"]], DrawArgument(Point<int16_t>(7, 17)));

        max_dimensions = normal_dimensions + Point<int16_t>(0, MAX_ADJ);
        update_dimensions();
    }

    // v83 frames are plain nine-slices: 6px rails, 29px (MinMap) and 72px
    // (MaxMap) title bands, 14/15px bottom bands and their own 1x1 center
    // fill. Only BtMap exists, so the collapsed bar is text plus button.
    void UIMiniMap::update_canvas_v83(Point<int16_t> map_dimensions)
    {
        nl::node min_src = minimap_node["Min"];

        const int16_t map_button_width = bt_map_width - 1;
        const int16_t mark_text_width = 48 + std::max(region_text.width(), town_text.width()) + 10;
        int16_t window_width = std::max<int16_t>(
            178,
            std::max<int16_t>(mark_text_width, map_dimensions.x() + minimap_nodes::SIDE_W * 2 + 8)
        );

        // Keep room for BtMap inside the title band.
        window_width = std::max<int16_t>(
            window_width,
            13 + combined_text_width + 6 + map_button_width + minimap_nodes::SIDE_W + minimap_nodes::MIN_E
        );

        const int16_t interior_width = window_width - minimap_nodes::SIDE_W * 2;
        const int16_t interior_height = std::max<int16_t>(map_dimensions.y(), 20) + 8;

        map_draw_origin_x = minimap_nodes::SIDE_W + std::max<int16_t>(0, (interior_width - map_dimensions.x()) / 2);
        map_draw_origin_y = minimap_nodes::NORMAL_TOP_H + std::max<int16_t>(0, (interior_height - map_dimensions.y()) / 2);

        middle_right_x = window_width - minimap_nodes::SIDE_W;

        const int16_t bar_width = minimap_nodes::min_bar_width(combined_text_width, map_button_width);
        const int16_t min_c_stretch = bar_width - minimap_nodes::MIN_W - minimap_nodes::MIN_E;
        min_sprites.emplace_back(min_src["w"], DrawArgument(Point<int16_t>(0, 0)));
        min_sprites.emplace_back(min_src["c"], DrawArgument(Point<int16_t>(minimap_nodes::MIN_W, 0), Point<int16_t>(min_c_stretch, 0)));
        min_sprites.emplace_back(min_src["e"], DrawArgument(Point<int16_t>(minimap_nodes::MIN_W + min_c_stretch, 0)));

        add_frame_sprites_v83(normal_sprites, minimap_node["MinMap"], interior_width, interior_height, minimap_nodes::NORMAL_TOP_H, minimap_nodes::NORMAL_BOTTOM_H);
        if (has_map)
        {
            normal_sprites.emplace_back(map_node["miniMap"]["canvas"], DrawArgument(Point<int16_t>(map_draw_origin_x, map_draw_origin_y)));
        }

        normal_dimensions = Point<int16_t>(
            window_width,
            minimap_nodes::NORMAL_TOP_H + interior_height + minimap_nodes::NORMAL_BOTTOM_H
        );

        add_frame_sprites_v83(max_sprites, minimap_node["MaxMap"], interior_width, interior_height, minimap_nodes::MAX_TOP_H, minimap_nodes::MAX_BOTTOM_H);
        if (has_map)
        {
            max_sprites.emplace_back(
                map_node["miniMap"]["canvas"],
                DrawArgument(Point<int16_t>(map_draw_origin_x, map_draw_origin_y + minimap_nodes::max_adjust()))
            );
        }

        max_sprites.emplace_back(
            map_helper_node["mark"][map_node["info"]["mapMark"]],
            DrawArgument(minimap_nodes::max_mark_pos())
        );

        max_dimensions = Point<int16_t>(
            window_width,
            minimap_nodes::MAX_TOP_H + interior_height + minimap_nodes::MAX_BOTTOM_H
        );
        update_dimensions();
    }

    // Nine-slice assembly shared by the v83 MinMap/MaxMap frames.
    void UIMiniMap::add_frame_sprites_v83(
        std::vector<Sprite>& sprites,
        nl::node src,
        int16_t interior_width,
        int16_t interior_height,
        int16_t top_height,
        int16_t bottom_height
    )
    {
        const int16_t side = minimap_nodes::SIDE_W;
        const int16_t right_x = side + interior_width;
        const int16_t body_y = top_height;

        sprites.emplace_back(src["c"], DrawArgument(Point<int16_t>(side, body_y), Point<int16_t>(interior_width, interior_height)));
        sprites.emplace_back(src["nw"], DrawArgument(Point<int16_t>(0, 0)));
        sprites.emplace_back(src["n"], DrawArgument(Point<int16_t>(side, 0), Point<int16_t>(interior_width, 0)));
        sprites.emplace_back(src["ne"], DrawArgument(Point<int16_t>(right_x, 0)));
        sprites.emplace_back(src["w"], DrawArgument(Point<int16_t>(0, body_y), Point<int16_t>(0, interior_height)));
        sprites.emplace_back(src["e"], DrawArgument(Point<int16_t>(right_x, body_y), Point<int16_t>(0, interior_height)));
        sprites.emplace_back(src["sw"], DrawArgument(Point<int16_t>(0, body_y + interior_height)));
        sprites.emplace_back(src["s"], DrawArgument(Point<int16_t>(side, body_y + interior_height), Point<int16_t>(interior_width, 0)));
        sprites.emplace_back(src["se"], DrawArgument(Point<int16_t>(right_x, body_y + interior_height)));
    }


    void UIMiniMap::update_dimensions()
    {
        if (type == MIN)
        {
            dimension = min_dimensions;
            return;
        }

        Point<int16_t> base = type == MAX ? max_dimensions : normal_dimensions;
        dimension = base;

        if (list_npc_enabled)
        {
            dimension += list_npc_dimensions;
            dimension.set_y(std::max<int16_t>(base.y(), list_npc_dimensions.y()));
        }
    }
}
