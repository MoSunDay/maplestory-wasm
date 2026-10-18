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
#include "UINpcTalkRefs.h"

#include "../../Console.h"
#include "../../Data/ItemData.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"
#include "nlnx/node.hpp"

#include <unordered_set>

namespace jrc
{
    namespace npctalk_text
    {
        namespace
        {
            std::string try_get_item_name(int32_t itemid)
            {
                const ItemData& item_data = ItemData::get(itemid);
                if (item_data.is_valid())
                {
                    return item_data.get_name();
                }

                return {};
            }

            std::string try_get_npc_name(int32_t npcid)
            {
                nl::node npc_name = nl::nx::string["Npc.img"][std::to_string(npcid)]["name"];
                if (npc_name)
                {
                    return npc_name.get_string();
                }

                return {};
            }

            std::string try_get_mob_name(int32_t mobid)
            {
                nl::node mob_name = nl::nx::string["Mob.img"][std::to_string(mobid)]["name"];
                if (mob_name)
                {
                    return mob_name.get_string();
                }

                return {};
            }

            std::string try_get_map_name(int32_t mapid)
            {
                const NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapid);
                if (!map_info.full_name.empty())
                {
                    return map_info.full_name;
                }

                return map_info.name;
            }
        }

        std::string resolve_prefixed_reference(char prefix, int32_t id)
        {
            switch (prefix)
            {
            case 'm':
            case 'M':
                return try_get_map_name(id);
            case 't':
            case 'T':
            case 'z':
            case 'Z':
                return try_get_item_name(id);
            case 'p':
            case 'P':
                return try_get_npc_name(id);
            case 'o':
            case 'O':
                return try_get_mob_name(id);
            default:
                return {};
            }
        }

        std::string resolve_numeric_reference(int32_t id)
        {
            if (id >= 100000000)
            {
                return try_get_map_name(id);
            }

            // Some server scripts send bare "#123456#" placeholders without a
            // token prefix. Resolve by trying the most common string tables.
            std::string resolved = try_get_item_name(id);
            if (!resolved.empty())
            {
                return resolved;
            }

            resolved = try_get_npc_name(id);
            if (!resolved.empty())
            {
                return resolved;
            }

            resolved = try_get_mob_name(id);
            if (!resolved.empty())
            {
                return resolved;
            }

            return try_get_map_name(id);
        }

        void log_unresolved_npc_token(const std::string& token)
        {
            static std::unordered_set<std::string> logged_tokens;
            static bool emitted_log_limit_message = false;
            constexpr size_t MAX_UNRESOLVED_TOKEN_LOGS = 20;

            if (logged_tokens.find(token) != logged_tokens.end())
            {
                return;
            }

            if (logged_tokens.size() >= MAX_UNRESOLVED_TOKEN_LOGS)
            {
                if (!emitted_log_limit_message)
                {
                    Console::get().print(
                        "[npc] too many unresolved dialogue tokens, suppressing further logs"
                    );
                    emitted_log_limit_message = true;
                }
                return;
            }

            logged_tokens.insert(token);
            Console::get().print("[npc] unresolved dialogue token: " + token);
        }
    }
}
