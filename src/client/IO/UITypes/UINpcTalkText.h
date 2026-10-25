//////////////////////////////////////////////////////////////////////////////
// Pure text-processing helpers for the NPC dialogue window.
//
// Split out of UINpcTalk.cpp so the dialogue window file only keeps the
// state machine and rendering. Everything here is a free function without
// client state: the token scanner resolves #p/#t/#m/#o/#q style references
// and #c item counts through the string/item/inventory tables, strips or
// expands the remaining formatting tokens and normalizes the leftover
// whitespace (UINpcTalkWhitespace.h).
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace jrc
{
    namespace npctalk_text
    {
        // How the scanners treat renderable sprite tokens: `#f<path>#` (and
        // `#F<path>#`) inline images plus the `#i`/`#v` item and `#s` skill
        // icon macros.
        enum class EmbedImages
        {
            STRIP,  // drop the whole token so no path/id text leaks
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
        // Sprite tokens (#f/#F paths, #i/#v items, #s skills) are always
        // removed from the visible output; KEEP additionally preserves them
        // verbatim for a later sprite extraction pass. The result is
        // whitespace-normalized (see UINpcTalkWhitespace.h).
        std::string strip_npc_tokens(
            const std::string& source,
            EmbedImages embed_images = EmbedImages::STRIP
        );

        // Expand #h/#p/#t/#m/#o/#c/#q references into names/counts, drop
        // formatting tokens and normalize the remaining whitespace.
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
