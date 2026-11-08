//////////////////////////////////////////////////////////////////////////////
// Style-list support for NPC dialogues of msgtype 7 (sendStyle).
//
// Salon coupons and the GM stylist (9900000) pick looks server-sided: the
// NPC_TALK packet appends a plain array of style ids (hair 30000-34999,
// face 20000-21999 plus small skin ids) instead of #L..#l option markup.
// The helpers below turn that array into the same SelectionList shape the
// text parser produces, so UINpcTalk can reuse its selection labels,
// scrolling and hover handling unchanged.
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkStyles.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

namespace jrc
{
    namespace npctalk_styles
    {
        namespace
        {
            constexpr int8_t STYLE_DIALOGUE_TYPE = 7;

            constexpr int32_t SKIN_ID_MAX = 999;
            constexpr int32_t HAIR_ID_MIN = 30000;
            constexpr int32_t HAIR_ID_MAX = 34999;
            constexpr int32_t FACE_ID_MIN = 20000;
            constexpr int32_t FACE_ID_MAX = 21999;

            // v83 String.nx has no name nodes for the tiny skin ids the GM
            // stylist sends, so resolve those from a small static table.
            const char* skin_name(int32_t skin_id)
            {
                switch (skin_id)
                {
                case 0:
                    return "亮肤色";
                case 1:
                    return "苍白";
                case 2:
                    return "深色";
                case 3:
                    return "灰白";
                case 4:
                    return "蓝色";
                case 5:
                    return "绿色";
                case 9:
                    return "粉色";
                case 10:
                    return "灰色";
                default:
                    return nullptr;
                }
            }

            // Read one Eqp look name; missing nodes safely read as an empty
            // string, the same pattern the Hair/Face look classes use.
            std::string look_table_name(const char* table, int32_t style_id)
            {
                return nl::nx::string["Eqp.img"]["Eqp"][table]
                    [std::to_string(style_id)]["name"].get_string();
            }
        }

        bool is_style_dialogue_type(int8_t msgtype)
        {
            return msgtype == STYLE_DIALOGUE_TYPE;
        }

        std::string resolve_style_name(int32_t style_id)
        {
            std::string name;

            if (style_id >= 0 && style_id <= SKIN_ID_MAX)
            {
                const char* skin = skin_name(style_id);
                if (skin)
                    name = skin;
            }
            else if (style_id >= HAIR_ID_MIN && style_id <= HAIR_ID_MAX)
            {
                name = look_table_name("Hair", style_id);
            }
            else if (style_id >= FACE_ID_MIN && style_id <= FACE_ID_MAX)
            {
                name = look_table_name("Face", style_id);
            }

            if (name.empty())
                name = "造型 #" + std::to_string(style_id);

            return name;
        }

        npctalk_text::SelectionList build_style_selection(
            const std::string& rendered_prompt,
            const std::vector<int32_t>& style_ids
        )
        {
            npctalk_text::SelectionList selection;
            selection.rendered_text = rendered_prompt;
            selection.option_ids.reserve(style_ids.size());
            selection.option_texts.reserve(style_ids.size());

            for (size_t i = 0; i < style_ids.size(); ++i)
            {
                selection.option_ids.push_back(static_cast<int32_t>(i));
                selection.option_texts.push_back(
                    resolve_style_name(style_ids[i]) + " (" +
                    std::to_string(style_ids[i]) + ")");
            }

            return selection;
        }
    }
}
