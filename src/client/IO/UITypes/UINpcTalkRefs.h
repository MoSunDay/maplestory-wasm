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
#pragma once

#include <cstdint>
#include <string>

namespace jrc
{
    namespace npctalk_text
    {
        // Resolves an npc-dialogue token prefix (#m/#p/#o/#t/#z ...) plus a
        // numeric id into a display name. Returns an empty string when no
        // matching entry exists.
        std::string resolve_prefixed_reference(char prefix, int32_t id);

        // Resolves a bare delimited id token (#123456#) into a display name,
        // probing the item, npc, mob and map string tables in that order.
        std::string resolve_numeric_reference(int32_t id);

        // Reports an unresolvable token once per distinct token text. A log
        // limit prevents console flooding on malformed dialogues.
        void log_unresolved_npc_token(const std::string& token);
    }
}
