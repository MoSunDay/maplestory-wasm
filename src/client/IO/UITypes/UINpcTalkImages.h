//////////////////////////////////////////////////////////////////////////////
// Inline-image macro support for NPC dialogue text.
//
// Servers embed small bitmaps into dialogue strings with `#f<path>#`, e.g.
// `#fUI/GuildMark/Mark/Pattern/00004014/11#`. Everything in this header is a
// free function without client state: token recognition, nx path resolution
// and the extraction pass that replaces tokens with space placeholders so the
// surrounding text keeps its original line wrapping.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Texture.h"

#include "nlnx/node.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    namespace npctalk_images
    {
        // One embedded image: the sprite, the byte index of its space
        // placeholder inside the cleaned dialogue text, and a small extra
        // x nudge (kept for future layout tweaks, always 0 today).
        struct NpcEmbedSprite
        {
            Texture texture;
            size_t byte_pos;
            int16_t x_offset;
        };

        // Heuristic recognizer for `#f<path>#` inline-image tokens. Returns
        // true (plus the path body and the index just past the closing '#')
        // when the body is non-empty, at most 120 bytes, contains no
        // whitespace and contains '/'. Anything else is not an image token
        // and keeps the legacy colour-token behaviour.
        bool try_parse_embed_token(
            const std::string& source,
            size_t cursor,
            std::string& out_path,
            size_t& out_end
        );

        // Map a `#f..#` path onto the nx data files. Only the UI and Effect
        // domains are supported; the first path segment is retried with an
        // appended ".img", and `UIWindow.img` falls back to `UIWindow2.img`
        // where newer data files moved the node. Returns a null node when no
        // bitmap resolves.
        nl::node resolve_embed_node(const std::string& path);

        // Remove every `#f..#` token from `source`, appending to `out_text`
        // a run of spaces as wide as the resolved sprite so line wrapping
        // matches the rendered result. Tokens that fail to resolve are
        // dropped without a placeholder (width 0). The returned sprites carry
        // the byte index of their placeholder run inside `out_text`.
        std::vector<NpcEmbedSprite> extract_embed_sprites(
            const std::string& source,
            std::string& out_text
        );
    }
}
