//////////////////////////////////////////////////////////////////////////////
// Style-list support for NPC dialogues of msgtype 7 (sendStyle); see
// UINpcTalkStyles.cpp for the details. Free functions only, no state.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "UINpcTalkText.h"

#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    namespace npctalk_styles
    {
        // True for the sendStyle dialogue type: the packet appends a raw
        // array of style ids instead of the #L..#l option markup.
        bool is_style_dialogue_type(int8_t msgtype);

        // Human readable label for one style id: String.nx names for the
        // hair/face id ranges, a static table for the small skin ids and a
        // numeric fallback for ids the tables do not know.
        std::string resolve_style_name(int32_t style_id);

        // Build the selection list for a sendStyle dialogue. option_ids are
        // the array indexes because servers take the index back (scripts do
        // e.g. hairnew[selection]); option_texts are the resolved style
        // names with the id appended so same-name colour variants stay
        // distinguishable.
        npctalk_text::SelectionList build_style_selection(
            const std::string& rendered_prompt,
            const std::vector<int32_t>& style_ids);
    }
}
