//////////////////////////////////////////////////////////////////////////////
// Inline-image macro support for NPC dialogue text.
//
// Servers embed small bitmaps into dialogue strings with `#f<path>#`, e.g.
// `#fUI/GuildMark/Mark/Pattern/00004014/11#`, and icon macros such as
// `#i<itemid>#`, `#v<itemid>#` and `#s<skillid>#`. Everything in this header
// is a free function without client state: token recognition, nx path
// resolution and the extraction pass that replaces tokens with space
// placeholders so the surrounding text keeps its original line wrapping.
// Item/skill textures come from the data caches through UINpcTalkIconData.h,
// which keeps this unit free of game-data includes for the standalone
// token/path verifier.
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
        // placeholder inside the cleaned dialogue text, a small extra
        // x nudge (kept for future layout tweaks, always 0 today), and the
        // on-screen size after optional icon capping. `width`/`height` are
        // what the placeholder reserves and what the draw pass scales the
        // native texture to; both equal the native texture dimensions for
        // `#f` path embeds, which are never capped.
        struct NpcEmbedSprite
        {
            Texture texture;
            size_t byte_pos;
            int16_t x_offset;
            int16_t width = 0;
            int16_t height = 0;
        };

        // Heuristic recognizer for `#f<path>#` inline-image tokens (uppercase
        // `#F<path>#` is accepted too; server markup lists both). Returns
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

        // Kind of icon referenced by an `#i`/`#v` (item) or `#s` (skill)
        // dialogue macro.
        enum class NpcIconKind
        {
            ITEM,
            SKILL
        };

        // Recognizer for the icon macros: `#i<itemid>#` and `#v<itemid>#`
        // (item picture) plus `#s<skillid>#` (skill picture). Uppercase
        // letters and a single optional ':' directly before the closing '#'
        // are accepted (server scripts are inconsistent). Returns true with
        // the kind, the id and the index just past the closing '#'; empty,
        // non-numeric and overflowing ids are rejected. `#q` (skill name) and
        // `#c` (item count) are text macros and are not recognized here.
        bool try_parse_icon_token(
            const std::string& source,
            size_t cursor,
            NpcIconKind& kind,
            int32_t& id,
            size_t& out_end
        );

        // Map a `#f..#` path onto the nx data files. The first segment names
        // the domain (any of the 16 top-level nx files, case-insensitive,
        // e.g. UI, Effect, Item, Map); the first path segment is retried
        // with an appended ".img", and `UIWindow.img` falls back to
        // `UIWindow2.img` where newer data files moved the node. Returns a
        // null node when no bitmap resolves.
        nl::node resolve_embed_node(const std::string& path);

        // On-screen size of an embedded sprite. `max_height` 0 keeps the
        // native size (used for `#f` path embeds); a positive cap shrinks
        // taller sprites to that height and derives the width from the
        // native aspect ratio with integer rounding (each axis at least
        // 1 px). Item/skill icons pass the A12M line height.
        void embed_display_size(
            int16_t native_width,
            int16_t native_height,
            int16_t max_height,
            int16_t& out_width,
            int16_t& out_height
        );

        // Top-left draw y (relative to the text block) for one embedded
        // sprite whose visual bottom must sit on the line baseline. Textures
        // are drawn around their origin, so a bottom-anchored item icon
        // (origin_y == native_height) is placed at the baseline itself,
        // while a top-left `#f` image (origin_y == 0) is placed at
        // baseline - display_height. Kept as a pure integer function so the
        // standalone verifier can cover icon and path-embed origins without
        // a graphics context.
        int16_t embed_sprite_y(
            int16_t line_baseline,
            int16_t native_height,
            int16_t origin_y,
            int16_t display_height
        );

        // Remove every `#f..#`, `#i/#v..#` and `#s..#` token from `source`,
        // appending to `out_text` a run of spaces as wide as the resolved
        // sprite so line wrapping matches the rendered result (caller passes
        // the advance width of a space for the dialogue font). Item and
        // skill icons are capped to one dialogue text line so option lists
        // stay readable; `#f` path embeds keep their native size. Tokens that
        // fail to resolve are dropped without a placeholder (width 0). The
        // returned sprites carry the byte index of their placeholder run
        // inside `out_text` and the on-screen size the draw pass must use.
        std::vector<NpcEmbedSprite> extract_embed_sprites(
            const std::string& source,
            int16_t advance_per_space,
            std::string& out_text
        );
    }
}
