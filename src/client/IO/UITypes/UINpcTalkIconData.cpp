//////////////////////////////////////////////////////////////////////////////
// Item/skill icon lookup for NPC dialogue sprite macros
// (see UINpcTalkIconData.h).
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkIconData.h"

#include "../../Data/ItemData.h"
#include "../../Data/SkillData.h"

namespace jrc
{
    namespace npctalk_images
    {
        const Texture* item_icon_texture(int32_t itemid)
        {
            const ItemData& item_data = ItemData::get(itemid);
            if (!item_data.is_valid())
            {
                return nullptr;
            }

            return &item_data.get_icon(false);
        }

        const Texture* skill_icon_texture(int32_t skillid)
        {
            const Texture& icon = SkillData::get(skillid).get_icon(SkillData::NORMAL);
            if (icon.width() <= 0)
            {
                return nullptr;
            }

            return &icon;
        }
    }
}
