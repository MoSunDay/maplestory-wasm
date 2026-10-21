//////////////////////////////////////////////////////////////////////////////
// Pure text-processing helpers for the NPC dialogue window.
//
// Split out of UINpcTalk.cpp so the dialogue window file only keeps the
// state machine and rendering. Everything here is a free function without
// client state: the token scanner resolves #p/#t/#m/#o style references
// through the string/item tables and strips or expands the remaining
// formatting tokens.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace jrc
{
    namespace npctalk_text
    {
        // How the scanners treat `#f<path>#` inline-image tokens.
        enum class EmbedImages
        {
            STRIP,  // drop the whole token so no path text leaks
            KEEP    // copy it through so the caller can extract sprites
        };

        struct SelectionList
        {
            std::string rendered_text;
            std::vector<std::string> option_texts;
            std::vector<int32_t> option_ids;
        };

        // True for the two message types servers use for #L..#l menus.
        bool is_selection_dialogue_type(int8_t msgtype);

        // Remove every #..# placeholder token, keeping the readable text.
        // Inline-image tokens are always removed from the output; KEEP
        // additionally preserves them for a later sprite extraction pass.
        std::string strip_npc_tokens(
            const std::string& source,
            EmbedImages embed_images = EmbedImages::STRIP
        );

        // Expand #h/#p/#t/#m/#o references into names, drop formatting tokens.
        std::string replace_macros(
            const std::string& source,
            EmbedImages embed_images = EmbedImages::STRIP
        );

        // Split a dialogue string into its prompt text and #L id#option#l list.
        SelectionList parse_selections(
            const std::string& source,
            EmbedImages embed_images = EmbedImages::STRIP
        );
    }
}
