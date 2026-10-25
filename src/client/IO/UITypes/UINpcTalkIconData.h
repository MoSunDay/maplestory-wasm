//////////////////////////////////////////////////////////////////////////////
// Item/skill icon lookup for NPC dialogue sprite macros.
//
// `#i<itemid>#`/`#v<itemid>#` (item picture) and `#s<skillid>#` (skill
// picture) resolve through the client data caches. Keeping that lookup in its
// own unit leaves UINpcTalkImages.cpp free of game-data includes, so the
// standalone token/path verifier can still compile and link it without the
// data caches (the extraction pass is dropped by --gc-sections there).
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Texture.h"

#include <cstdint>

namespace jrc
{
    namespace npctalk_images
    {
        // Returns the item's icon (the variant without the drop shadow), or
        // nullptr when the id is not a valid item. The returned reference
        // stays valid while the data cache lives.
        const Texture* item_icon_texture(int32_t itemid);

        // Returns the skill's normal icon, or nullptr when the id has no
        // loaded skill icon.
        const Texture* skill_icon_texture(int32_t skillid);
    }
}
