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
#include "UIWorldMap.h"
#include "UIWorldMapNodes.h"

#include "../UI.h"
#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Gameplay/Stage.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        // MapLink 子节点按名称数字排序（"0","1",...,"12"）；NX 的字符串
        // 排序（"0","1","10","11","12","2",...）会让按钮错位、并把
        // "7"/"8"/"9" 挤出 BT_LINK 上限之外导致区域不可点击。
        // 限长 <=9 位纯数字才数值比较：损坏 NX 的超长数字名会让
        // std::stol 抛 out_of_range，回退字符串序即可。
        bool link_node_order(const nl::node& a, const nl::node& b)
        {
            const std::string an = a.name();
            const std::string bn = b.name();
            const auto numeric = [](const std::string& name) {
                return !name.empty() && name.length() <= 9
                    && name.find_first_not_of("0123456789")
                        == std::string::npos;
            };
            const bool anum = numeric(an);
            const bool bnum = numeric(bn);
            if (anum && bnum)
                return std::stol(an) < std::stol(bn);
            if (anum != bnum)
                return anum;
            return an < bn;
        }

        // mapid 前缀节点（"WorldMap0" + mapid/10000000）在 NX 中缺失时，
        // 扫描各区域节点的 MapList 反查包含该地图的最深归属：深度 = 沿
        // info/parentMap 链回到 "WorldMap" 根的跳数（环保护上限 8）。
        // 22x/23x/24x 等无专属前缀节点的地图由此落到玩具城/水下/神木等
        // 真实区域，而不是渲染根图却在 resolved/user 里记一个假名。
        std::string deepest_region_map(int32_t mapid)
        {
            const nl::node root = worldmap_nodes::worldmap_data();
            std::string best;
            int32_t best_depth = -1;
            for (nl::node region : root)
            {
                const std::string name = region.name();
                if (name.length() < 5
                    || name.compare(name.length() - 4, 4, ".img") != 0
                    || name == "WorldMap.img")
                {
                    continue;
                }

                bool contains = false;
                for (nl::node list : region["MapList"])
                {
                    for (nl::node map_no : list["mapNo"])
                    {
                        if (static_cast<int32_t>(map_no) == mapid)
                        {
                            contains = true;
                            break;
                        }
                    }
                    if (contains)
                    {
                        break;
                    }
                }
                if (!contains)
                {
                    continue;
                }

                int32_t depth = 0;
                std::string parent
                    = region["info"]["parentMap"].get_string();
                while (depth < 8 && !parent.empty() && parent != "WorldMap")
                {
                    nl::node parent_node = root[parent + ".img"];
                    if (!parent_node)
                    {
                        break;
                    }
                    parent = parent_node["info"]["parentMap"].get_string();
                    ++depth;
                }

                if (depth > best_depth)
                {
                    best_depth = depth;
                    best = name.substr(0, name.length() - 4);
                }
            }
            return best;
        }
    }

    UIWorldMap::UIWorldMap()
        : UIDragElement<PosMAP>(Point<int16_t>()),
          search(true),
          show_path_img(false),
          mapid(-1),
          search_text_dim(82, 14)
    {
        nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
        nl::node world_map = worldmap_nodes::src();
        nl::node world_map_search = world_map["WorldMapSearch"];
        nl::node background = world_map_search["backgrnd"];
        nl::node helper = nl::nx::map["MapHelper.img"]["worldMap"];

        // The post-Chaos frame is one ready-made sprite; the v83 frame is
        // assembled from nine-slice pieces in draw().
        if (worldmap_nodes::post_chaos())
        {
            sprites.emplace_back(world_map["Border"]["0"]);
        }

        cur_pos = helper["curPos"];
        for (size_t i = 0; i < MAPSPOT_TYPE_MAX; ++i)
        {
            npc_pos[i] = helper["npcPos" + std::to_string(i)];
        }

        search_background = background;
        search_notice = world_map_search["notice"];

        bg_dimensions = worldmap_nodes::post_chaos()
            ? Texture(world_map["Border"]["0"]).get_dimensions()
            : worldmap_nodes::frame_dimensions();
        bg_search_dimensions = search_background.get_dimensions();
        background_dimensions = worldmap_nodes::has_search()
            ? Point<int16_t>(bg_dimensions.x(), 0)
            : Point<int16_t>();
        base_position = worldmap_nodes::post_chaos()
            ? Point<int16_t>(bg_dimensions.x() / 2, bg_dimensions.y() / 2 + 15)
            : worldmap_nodes::spot_base();

        Point<int16_t> close_pos(bg_dimensions.x() - 22, 4);
        buttons[BT_CLOSE] = std::make_unique<MapleButton>(close, close_pos);
        buttons[BT_SEARCH] = std::make_unique<MapleButton>(world_map["BtSearch"]);
        buttons[BT_AUTOFLY] = std::make_unique<MapleButton>(world_map["BtAutoFly_1"]);
        buttons[BT_NAVIREG] = std::make_unique<MapleButton>(world_map["BtNaviRegister"]);
        buttons[BT_SEARCH_CLOSE] = std::make_unique<MapleButton>(close, close_pos + Point<int16_t>(bg_search_dimensions.x(), 0));
        buttons[BT_ALLSEARCH] = std::make_unique<MapleButton>(world_map_search["BtAllsearch"], background_dimensions);

        // The v83 asset set has no search/auto-fly/register art; keep the
        // null-node buttons dormant so set_search() can still reach them.
        if (!worldmap_nodes::has_search())
        {
            buttons[BT_SEARCH]->set_active(false);
            buttons[BT_AUTOFLY]->set_active(false);
            buttons[BT_NAVIREG]->set_active(false);
        }

        Point<int16_t> search_pos = background_dimensions + Point<int16_t>(13, 25);
        search_text = Textfield(
            Text::A11M,
            Text::LEFT,
            Text::BLACK,
            Rectangle<int16_t>(search_pos, search_pos + search_text_dim),
            8
        );

        set_search(worldmap_nodes::has_search());
        dragarea = Point<int16_t>(bg_dimensions.x(), 20);
    }

    void UIWorldMap::draw(float alpha) const
    {
        if (worldmap_nodes::post_chaos())
        {
            UIElement::draw_sprites(alpha);
        }
        else
        {
            worldmap_nodes::draw_frame(position);
        }

        if (search)
        {
            search_background.draw(position + background_dimensions);
            search_notice.draw(position + background_dimensions);
            search_text.draw(position);
        }

        base_img.draw(position + worldmap_nodes::image_draw_pos(base_position));

        for (const auto& entry : buttons)
        {
            if (entry.first >= BT_LINK0 && entry.second && entry.second->get_state() == Button::MOUSEOVER)
            {
                auto image = link_images.find(entry.first);
                if (image != link_images.end())
                {
                    image->second.draw(position + base_position);
                    break;
                }
            }
        }

        if (show_path_img)
        {
            path_img.draw(position + base_position);
        }

        bool found = false;
        for (const auto& spot : map_spots)
        {
            spot.second.marker.draw(position + base_position + spot.first);

            if (found)
            {
                continue;
            }

            for (int32_t spot_mapid : spot.second.map_ids)
            {
                if (spot_mapid == mapid)
                {
                    found = true;
                    if (spot.second.type < MAPSPOT_TYPE_MAX)
                    {
                        npc_pos[spot.second.type].draw(position + base_position + spot.first, alpha);
                    }
                    cur_pos.draw(position + base_position + spot.first, alpha);
                    break;
                }
            }
        }

        UIElement::draw_buttons(alpha);
    }

    void UIWorldMap::update()
    {
        int32_t current_mapid = Stage::get().get_mapid();
        if (current_mapid != mapid)
        {
            mapid = current_mapid;

            int32_t prefix = mapid / 10000000;
            user_map = "WorldMap0" + std::to_string(prefix);
            if (!worldmap_nodes::worldmap_data()[user_map + ".img"]["BaseImg"]["0"])
            {
                // 前缀节点缺失（22x/23x/24x 等无对应节点）：按 MapList
                // 反查真实归属区域，查不到才回落世界根图。
                const std::string region = deepest_region_map(mapid);
                user_map = region.empty() ? "WorldMap" : region;
            }
            update_world(user_map);
        }

        search_text.update(position);

        for (size_t i = 0; i < MAPSPOT_TYPE_MAX; ++i)
        {
            npc_pos[i].update();
        }

        cur_pos.update();
        UIElement::update();
    }

    void UIWorldMap::toggle_active()
    {
        bool was_active = active;
        UIElement::toggle_active();

        if (was_active && !active && search_text.get_state() == Textfield::FOCUSED)
        {
            UI::get().focus_textfield(nullptr);
        }

        if (!active)
        {
            set_search(worldmap_nodes::has_search());
        }

        // Re-resolve on every toggle so a first open before the field
        // data arrived (or a transient resolve failure) never leaves the
        // window blank until the next map change.
        update_world(user_map);
    }

    bool UIWorldMap::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        bool removed = UIDragElement::remove_cursor(clicked, cursorpos);

        if (!is_in_range(cursorpos))
        {
            UI::get().clear_tooltip(Tooltip::WORLDMAP);
            show_path_img = false;
        }

        return removed;
    }

    UIElement::CursorResult UIWorldMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            return UIDragElement::send_cursor(clicked, cursorpos);
        }

        Cursor::State text_state = search_text.send_cursor(cursorpos, clicked);
        if (text_state != Cursor::IDLE)
        {
            return { text_state, true };
        }

        show_path_img = false;

        for (const auto& path : map_spots)
        {
            Point<int16_t> marker_lt = path.first + position + base_position - Point<int16_t>(10, 10);
            Point<int16_t> marker_rb = marker_lt + path.second.marker.get_dimensions();
            Rectangle<int16_t> bounds(marker_lt, marker_rb);

            if (!clicked && bounds.contains(cursorpos))
            {
                path_img = path.second.path;
                show_path_img = path_img.is_valid();

                UI::get().show_map(
                    Tooltip::WORLDMAP,
                    path.second.title,
                    path.second.description,
                    path.second.map_ids.empty() ? 0 : path.second.map_ids.front(),
                    path.second.bolded,
                    false
                );
                break;
            }
        }

        // 区域链接矩形大面积互相重叠（如 ElNath 的 278x217 高亮盖住
        // 玩具城岛屿），基类"首个命中矩形获胜"的分发会点错区域。这里
        // 先按 linkImg 的 alpha 形状精确判定；未命中任何形状时临时屏蔽
        // 链接按钮再走基类分发，避免矩形吞掉关闭/搜索等按钮的点击。
        const uint64_t press_id = UI::get().get_cursor_press_id();
        const int16_t hit = link_hit(cursorpos);
        if (hit >= 0)
        {
            Button& button = *buttons[hit];
            if (clicked)
            {
                if (handled_button_press_id == press_id)
                {
                    return { Cursor::CLICKING, true };
                }

                handled_button_press_id = press_id;
                if (button.get_state() == Button::NORMAL)
                {
                    Sound(Sound::BUTTONOVER).play();
                }

                Sound(Sound::BUTTONCLICK).play();
                button.set_state(button_pressed(hit));
                return { Cursor::CLICKING, true };
            }

            for (const auto& entry : link_maps)
            {
                auto iter = buttons.find(entry.first);
                if (iter == buttons.end() || !iter->second)
                {
                    continue;
                }
                if (entry.first == hit)
                {
                    if (iter->second->get_state() == Button::NORMAL)
                    {
                        Sound(Sound::BUTTONOVER).play();
                        iter->second->set_state(Button::MOUSEOVER);
                    }
                }
                else if (iter->second->get_state() == Button::MOUSEOVER)
                {
                    iter->second->set_state(Button::NORMAL);
                }
            }
            return { Cursor::CANCLICK, true };
        }

        for (const auto& entry : link_maps)
        {
            auto iter = buttons.find(entry.first);
            if (iter != buttons.end() && iter->second)
            {
                iter->second->set_active(false);
            }
        }
        const UIElement::CursorResult result =
            UIDragElement::send_cursor(clicked, cursorpos);
        for (const auto& entry : link_maps)
        {
            auto iter = buttons.find(entry.first);
            if (iter != buttons.end() && iter->second)
            {
                iter->second->set_active(true);
            }
        }
        return result;
    }

    void UIWorldMap::send_key(int32_t, bool pressed, bool escape)
    {
        if (!pressed || !escape)
        {
            return;
        }

        if (search)
        {
            set_search(false);
        }
        else if (parent_map.empty())
        {
            toggle_active();
        }
        else
        {
            update_world(parent_map);
        }
    }

    Button::State UIWorldMap::button_pressed(uint16_t buttonid)
    {
        if (buttonid >= BT_LINK0)
        {
            auto iter = link_maps.find(buttonid);
            if (iter != link_maps.end())
            {
                update_world(iter->second);
            }
            return Button::IDENTITY;
        }

        switch (buttonid)
        {
        case BT_CLOSE:
            toggle_active();
            return Button::NORMAL;
        case BT_SEARCH:
            set_search(!search);
            return Button::NORMAL;
        case BT_SEARCH_CLOSE:
            set_search(false);
            return Button::NORMAL;
        default:
            return Button::DISABLED;
        }
    }

    void UIWorldMap::set_search(bool enable)
    {
        if (!enable && search_text.get_state() == Textfield::FOCUSED)
        {
            UI::get().focus_textfield(nullptr);
        }

        enable = enable && worldmap_nodes::has_search();
        search = enable;
        buttons[BT_SEARCH_CLOSE]->set_active(enable);
        buttons[BT_ALLSEARCH]->set_active(enable);
        search_text.set_state(enable ? Textfield::NORMAL : Textfield::DISABLED);
        dimension = enable ? bg_dimensions + Point<int16_t>(bg_search_dimensions.x(), 0) : bg_dimensions;
    }

    std::vector<uint8_t> UIWorldMap::link_alpha_mask(nl::node image)
    {
        std::vector<uint8_t> mask;
        if (image.data_type() != nl::node::type::bitmap)
        {
            return mask;
        }

        const nl::bitmap bmp = image;
        if (!bmp)
        {
            return mask;
        }

        const uint16_t width = bmp.width();
        const uint16_t height = bmp.height();
        const uint8_t* data = static_cast<const uint8_t*>(bmp.data());
        if (!data)
        {
            // 位图数据尚未就绪时返回空掩码，命中判定退回矩形。
            return mask;
        }

        const size_t count = static_cast<size_t>(width) * height;
        mask.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            mask[i] = data[i * 4 + 3] > 0 ? 1 : 0;
        }
        return mask;
    }

    int16_t UIWorldMap::link_hit(Point<int16_t> cursorpos) const
    {
        int16_t best = -1;
        int32_t best_area = 0;
        for (const auto& entry : link_maps)
        {
            auto iter = buttons.find(entry.first);
            if (iter == buttons.end() || !iter->second || !iter->second->is_active())
            {
                continue;
            }

            const Rectangle<int16_t> bounds = iter->second->bounds(position);
            if (!bounds.contains(cursorpos))
            {
                continue;
            }

            auto mask = link_masks.find(entry.first);
            if (mask != link_masks.end() && !mask->second.empty())
            {
                const int16_t local_x = cursorpos.x() - bounds.l();
                const int16_t local_y = cursorpos.y() - bounds.t();
                const int32_t width = bounds.width();
                const size_t index =
                    static_cast<size_t>(local_y) * width + local_x;
                if (index >= mask->second.size() || !mask->second[index])
                {
                    continue;
                }
            }

            const int32_t area =
                static_cast<int32_t>(bounds.width()) * bounds.height();
            if (best < 0 || area < best_area)
            {
                best = entry.first;
                best_area = area;
            }
        }
        return best;
    }

    void UIWorldMap::update_world(std::string map)
    {
        nl::node world_map = worldmap_nodes::worldmap_data()[map + ".img"];
        if (!world_map["BaseImg"]["0"])
        {
            // Missing region data falls back to the whole-world image
            // instead of rendering an empty frame.
            world_map = worldmap_nodes::worldmap_data()["WorldMap.img"];
        }

        base_img = world_map["BaseImg"][0];
        parent_map = world_map["info"]["parentMap"].get_string();
        resolved_map = map;

        // v83 world-map images are origin-less; markers are positioned
        // relative to the image center instead of the post-Chaos frame.
        if (!worldmap_nodes::post_chaos())
        {
            const Point<int16_t> base_size = base_img.get_dimensions();
            const Point<int16_t> content = worldmap_nodes::content_pos();
            base_position = {
                static_cast<int16_t>(content.x() + base_size.x() / 2),
                static_cast<int16_t>(content.y() + base_size.y() / 2)
            };
        }

        link_images.clear();
        link_maps.clear();
        link_masks.clear();

        for (auto& button : buttons)
        {
            if (button.first >= BT_LINK0 && button.second)
            {
                button.second->set_active(false);
            }
        }

        std::vector<nl::node> link_nodes;
        for (nl::node link : world_map["MapLink"])
        {
            link_nodes.push_back(link);
        }
        std::sort(link_nodes.begin(), link_nodes.end(), link_node_order);

        uint16_t link_id = BT_LINK0;
        for (const nl::node& link : link_nodes)
        {
            if (link_id > BT_LINK15)
            {
                break;
            }

            nl::node link_info = link["link"];
            Texture link_image = link_info["linkImg"];

            link_images[link_id] = link_image;
            link_maps[link_id] = link_info["linkMap"].get_string();
            link_masks[link_id] = link_alpha_mask(link_info["linkImg"]);
            buttons[link_id] = std::make_unique<AreaButton>(
                base_position - link_image.get_origin(),
                link_image.get_dimensions()
            );
            buttons[link_id]->set_active(true);
            ++link_id;
        }

        nl::node map_image = nl::nx::map["MapHelper.img"]["worldMap"]["mapImage"];
        map_spots.clear();

        for (nl::node list : world_map["MapList"])
        {
            std::string description = list["desc"];
            std::string title = list["title"];
            uint8_t type = static_cast<uint8_t>(static_cast<int32_t>(list["type"]));
            Texture marker = map_image[std::to_string(type)];

            std::vector<int32_t> map_ids;
            for (nl::node map_no : list["mapNo"])
            {
                map_ids.push_back(map_no);
            }

            bool bolded = !description.empty();

            if (description.empty() && title.empty() && !map_ids.empty())
            {
                NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(map_ids.front());
                description = map_info.description;
                title = map_info.full_name;
                bolded = !description.empty();
            }

            map_spots.emplace_back(
                Point<int16_t>(list["spot"]),
                MapSpot{
                    description,
                    Texture(list["path"]),
                    title,
                    type,
                    marker,
                    bolded,
                    map_ids
                }
            );
        }
    }

    Point<int16_t> UIWorldMap::get_search_pos() const
    {
        return position + background_dimensions + Point<int16_t>(13, 25);
    }

    int16_t UIWorldMap::debug_geometry(int which) const
    {
        switch (which)
        {
        case 0:
            return position.x();
        case 1:
            return position.y();
        case 2:
            return dimension.x();
        default:
            return dimension.y();
        }
    }

    size_t UIWorldMap::debug_link_count() const
    {
        return link_maps.size();
    }

    int32_t UIWorldMap::debug_link_bounds(int index, int corner) const
    {
        auto iter = buttons.find(static_cast<uint16_t>(BT_LINK0 + index));
        if (iter == buttons.end() || !iter->second)
        {
            return -1;
        }

        Rectangle<int16_t> bounds = iter->second->bounds(position);
        switch (corner)
        {
        case 0:
            return bounds.l();
        case 1:
            return bounds.t();
        case 2:
            return bounds.r();
        default:
            return bounds.b();
        }
    }

    int32_t UIWorldMap::debug_link_state(int index) const
    {
        auto iter = buttons.find(static_cast<uint16_t>(BT_LINK0 + index));
        if (iter == buttons.end() || !iter->second)
        {
            return -1;
        }
        return static_cast<int32_t>(iter->second->get_state());
    }

    uint32_t UIWorldMap::debug_name_hash(const std::string& name)
    {
        uint32_t hash = 2166136261u;
        for (char c : name)
        {
            hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
        }
        return hash;
    }

    uint32_t UIWorldMap::debug_resolved_hash() const
    {
        return debug_name_hash(resolved_map);
    }

    uint32_t UIWorldMap::debug_user_hash() const
    {
        return debug_name_hash(user_map);
    }

    uint32_t UIWorldMap::debug_parent_hash() const
    {
        return debug_name_hash(parent_map);
    }

    uint32_t UIWorldMap::debug_link_target_hash(int index) const
    {
        auto iter = link_maps.find(static_cast<uint16_t>(BT_LINK0 + index));
        return iter != link_maps.end() ? debug_name_hash(iter->second) : 0;
    }

    bool UIWorldMap::debug_search() const
    {
        return search;
    }

    int16_t UIWorldMap::debug_link_hit(int32_t x, int32_t y) const
    {
        const int16_t hit = link_hit(
            { static_cast<int16_t>(x), static_cast<int16_t>(y) }
        );
        return hit >= BT_LINK0 ? static_cast<int16_t>(hit - BT_LINK0) : -1;
    }
}
