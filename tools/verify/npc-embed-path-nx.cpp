// Standalone verifier for the NPC dialogue sprite macros: `#f<path>#`
// inline-image token heuristics, `#i/#v/#s` icon token recognition and
// cross-nx-domain path resolution (UINpcTalkImages).
//
// No graphics state is needed: only the parsers and `resolve_embed_node` are
// kept by --gc-sections, so Texture/FontCache stay unlinked. The sprite
// extraction pass is never called here and is dropped together with its
// item/skill texture lookups (those live in UINpcTalkIconData.cpp).
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing
//       -Wno-unused-function -ffunction-sections -fdata-sections
//       -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes
//       tools/verify/npc-embed-path-nx.cpp
//       src/client/IO/UITypes/UINpcTalkImages.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp src/nlnx/nx.cpp
//       src/client/includes/lz4.c
//       -Wl,--gc-sections -o /tmp/npc-embed-path-nx
//   /tmp/npc-embed-path-nx assets/UI.nx assets/Effect.nx assets/Item.nx assets/Map.nx
//
// Asserts:
//   * `#f...#`/`#F...#` recognition accepts path-like bodies and rejects
//     colour-token lookalikes (`#f`, spaces, missing '/', over-long bodies);
//   * `#i/#v/#s` icon recognition accepts both letter cases, leading zeros and
//     an optional trailing ':', and rejects `#q`/`#c`, empty or non-numeric
//     bodies, missing closing '#' and overflowing ids;
//   * UI paths without ".img" and the UIWindow.img->UIWindow2.img QuestIcon
//     relocation still resolve to bitmaps;
//   * every nx domain used by server scripts (UI/Effect/Item/Map here)
//     resolves through its own file and maps onto a bitmap of known size;
//   * embedded-sprite sizes cap item/skill icons to the dialogue line height
//     while `#f` path embeds (max height 0) keep their native size;
//   * embedded-sprite placement puts the visual bottom on the line baseline
//     both for bottom-anchored icons and top-left `#f` images;
//   * unknown domains and missing nodes return a null node.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "IO/UITypes/UINpcTalkImages.h"

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

// Metadata-only bitmap implementation; the real decoder lives in the client.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
    uint16_t bitmap::width() const { return m_width; }
    uint16_t bitmap::height() const { return m_height; }
}

namespace nl
{
    namespace nx
    {
        // Defined in nx.cpp; keeps the opened files alive and returns the root.
        node add_file(std::string name);
    }
}

namespace
{
    using jrc::npctalk_images::NpcIconKind;
    using jrc::npctalk_images::embed_display_size;
    using jrc::npctalk_images::embed_sprite_y;
    using jrc::npctalk_images::resolve_embed_node;
    using jrc::npctalk_images::try_parse_embed_token;
    using jrc::npctalk_images::try_parse_icon_token;

    struct TokenCase
    {
        const char* source;
        size_t cursor;
        bool accepted;
        const char* path;
    };

    struct IconCase
    {
        const char* source;
        size_t cursor;
        bool accepted;
        NpcIconKind kind;
        int32_t id;
    };

    struct NodeCase
    {
        const char* path;
        int16_t width;
        int16_t height;
    };

    struct SizeCase
    {
        const char* label;
        int16_t native_width;
        int16_t native_height;
        int16_t max_height;
        int16_t expected_width;
        int16_t expected_height;
    };

    struct SpriteYCase
    {
        const char* label;
        int16_t line_baseline;
        int16_t native_height;
        int16_t origin_y;
        int16_t display_height;
        int16_t expected_y;
    };

    std::vector<std::string> failures;

    void check(bool ok, const std::string& label)
    {
        if (!ok)
        {
            failures.push_back(label);
            std::cout << "FAIL  " << label << '\n';
        }
    }

    void run_token_cases()
    {
        const TokenCase cases[] =
        {
            { "#fUI/GuildMark/Mark/Pattern/00004014/11#", 0, true,
              "UI/GuildMark/Mark/Pattern/00004014/11" },
            { "#fEffect/CharacterEff/1062114/1/0#", 0, true,
              "Effect/CharacterEff/1062114/1/0" },
            // Server markup also lists uppercase `#F<path>#`.
            { "#FUI/UIWindow.img/QuestIcon/3#", 0, true,
              "UI/UIWindow.img/QuestIcon/3" },
            { "before#fItem/Consume/0200.img/02000000/info/icon#after", 6,
              true, "Item/Consume/0200.img/02000000/info/icon" },
            // Colour-token lookalikes and malformed bodies stay non-image.
            { "#f", 0, false, "" },
            { "#f#", 0, false, "" },
            { "#fno-slash#", 0, false, "" },
            { "#fhas space/x#", 0, false, "" },
            { "#fhas\ttab/x#", 0, false, "" },
            { "#b", 0, false, "" },
            // ## escapes a literal '#', it must not start an image token.
            { "##fUI/x#", 0, false, "" }
        };

        for (const TokenCase& test : cases)
        {
            const std::string source = test.source;
            std::string path;
            size_t end = 0;
            const bool accepted =
                try_parse_embed_token(source, test.cursor, path, end);
            check(accepted == test.accepted,
                std::string("token accept: ") + test.source);
            if (!accepted)
            {
                continue;
            }

            check(path == test.path,
                std::string("token path: ") + test.source);
            check(end <= source.size() && source[end - 1] == '#',
                std::string("token end: ") + test.source);
        }

        // Body length boundary: 120 bytes is the documented ceiling.
        const std::string head = "#f";
        const std::string fitting(120 - 2, 'a');
        const std::string long_body(121 - 2, 'a');
        std::string path;
        size_t end = 0;
        check(try_parse_embed_token(head + fitting + "/x#", 0, path, end),
            "token length: 120 bytes accepted");
        check(!try_parse_embed_token(head + long_body + "/x#", 0, path, end),
            "token length: over 120 bytes rejected");
    }

    void run_icon_cases()
    {
        const IconCase cases[] =
        {
            // Item pictures: #i and #v, both letter cases.
            { "#i4000000#", 0, true, NpcIconKind::ITEM, 4000000 },
            { "#v4000000#", 0, true, NpcIconKind::ITEM, 4000000 },
            { "#I4000000#", 0, true, NpcIconKind::ITEM, 4000000 },
            { "#V4000000#", 0, true, NpcIconKind::ITEM, 4000000 },
            // Skill pictures: #s/#S.
            { "#s1000#", 0, true, NpcIconKind::SKILL, 1000 },
            { "#S1000#", 0, true, NpcIconKind::SKILL, 1000 },
            // Leading zeros and the optional ':' before the closing '#'.
            { "#i00004000000#", 0, true, NpcIconKind::ITEM, 4000000 },
            { "#i4000000:#", 0, true, NpcIconKind::ITEM, 4000000 },
            { "#s0001000:#", 0, true, NpcIconKind::SKILL, 1000 },
            // Token starting inside surrounding text.
            { "take#v2000000#now", 4, true, NpcIconKind::ITEM, 2000000 },
            // Text macros and malformed bodies are not icon tokens.
            { "#q1000#", 0, false, NpcIconKind::SKILL, 0 },
            { "#c4000000#", 0, false, NpcIconKind::ITEM, 0 },
            { "#i#", 0, false, NpcIconKind::ITEM, 0 },
            { "#s#", 0, false, NpcIconKind::SKILL, 0 },
            { "#i", 0, false, NpcIconKind::ITEM, 0 },
            { "#i4000000", 0, false, NpcIconKind::ITEM, 0 },
            { "#iabc#", 0, false, NpcIconKind::ITEM, 0 },
            { "#i4000000::#", 0, false, NpcIconKind::ITEM, 0 },
            { "#i4000000:x#", 0, false, NpcIconKind::ITEM, 0 },
            { "#x1#", 0, false, NpcIconKind::ITEM, 0 },
            { "#99999999999999999999#", 0, false, NpcIconKind::ITEM, 0 },
            { "#i99999999999999999999#", 0, false, NpcIconKind::ITEM, 0 }
        };

        for (const IconCase& test : cases)
        {
            const std::string source = test.source;
            NpcIconKind kind = NpcIconKind::ITEM;
            int32_t id = 0;
            size_t end = 0;
            const bool accepted =
                try_parse_icon_token(source, test.cursor, kind, id, end);
            check(accepted == test.accepted,
                std::string("icon accept: ") + test.source);
            if (!accepted)
            {
                continue;
            }

            check(kind == test.kind,
                std::string("icon kind: ") + test.source);
            check(id == test.id,
                std::string("icon id: ") + test.source);
            check(end <= source.size() && end > test.cursor &&
                source[end - 1] == '#',
                std::string("icon end: ") + test.source);
        }
    }

    // Item/skill icons pass the A12M line height; `#f` path embeds pass 0 so
    // a 142x18 quest icon keeps its native size instead of shrinking to 17.
    void run_size_cases()
    {
        const SizeCase cases[] =
        {
            { "item icon 32x32", 32, 32, 17, 17, 17 },
            { "consumable icon 27x30", 27, 30, 17, 15, 17 },
            { "weapon icon 35x32", 35, 32, 17, 19, 17 },
            { "wide icon 40x39", 40, 39, 17, 17, 17 },
            { "narrow icon 33x27", 33, 27, 17, 21, 17 },
            { "skill icon 26x30", 26, 30, 17, 15, 17 },
            { "path embed 142x18 native", 142, 18, 0, 142, 18 },
            { "path embed 11x10 native", 11, 10, 0, 11, 10 },
            { "small icon already fits", 11, 10, 17, 11, 10 },
            { "path embed would cap at 17", 142, 18, 17, 134, 17 },
            { "one pixel minimum width", 1, 32, 17, 1, 17 },
            { "degenerate native height", 0, 0, 17, 0, 0 }
        };

        for (const SizeCase& test : cases)
        {
            int16_t width = -1;
            int16_t height = -1;
            embed_display_size(
                test.native_width, test.native_height, test.max_height,
                width, height
            );
            check(width == test.expected_width && height == test.expected_height,
                std::string("display size: ") + test.label);
        }
    }

    // Texture origins are anchor rows counted from the top of the bitmap, so
    // a bottom-anchored item icon (origin == height) draws exactly on the
    // baseline while a top-left `#f` image draws one display height above it.
    void run_sprite_y_cases()
    {
        const SpriteYCase cases[] =
        {
            { "item icon 32x32", 17, 32, 32, 17, 17 },
            { "path embed top-left", 17, 15, 0, 15, 2 },
            { "weapon icon 35x32 origin 1", 17, 32, 1, 17, 1 },
            { "wide icon 39x39 origin 38", 17, 39, 38, 17, 17 },
            { "wide icon 39x39 origin 37", 17, 39, 37, 17, 16 },
            { "centred origin native size", 17, 30, 15, 30, 2 },
            { "unscaled icon origin bottom", 17, 26, 26, 26, 17 },
            { "degenerate native height", 17, 0, 0, 12, 5 }
        };

        for (const SpriteYCase& test : cases)
        {
            const int16_t actual = embed_sprite_y(
                test.line_baseline, test.native_height,
                test.origin_y, test.display_height
            );
            check(actual == test.expected_y,
                std::string("sprite y: ") + test.label);
        }
    }

    void check_bitmap(const NodeCase& test)
    {
        const nl::node node = resolve_embed_node(test.path);
        check(static_cast<bool>(node),
            std::string("resolve: ") + test.path);
        if (!node)
        {
            return;
        }

        check(node.data_type() == nl::node::type::bitmap,
            std::string("bitmap type: ") + test.path);
        const nl::bitmap bitmap = node.get_bitmap();
        std::cout << "  " << test.path << " -> " << bitmap.width()
                  << 'x' << bitmap.height() << '\n';
        check(bitmap.width() == test.width && bitmap.height() == test.height,
            std::string("bitmap size: ") + test.path);
    }
}

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::cout << "usage: " << argv[0]
                  << " <UI.nx> <Effect.nx> <Item.nx> <Map.nx>\n";
        return 2;
    }

    nl::nx::ui = nl::nx::add_file(argv[1]);
    nl::nx::effect = nl::nx::add_file(argv[2]);
    nl::nx::item = nl::nx::add_file(argv[3]);
    nl::nx::map = nl::nx::add_file(argv[4]);

    run_token_cases();
    run_icon_cases();
    run_size_cases();
    run_sprite_y_cases();

    const NodeCase nodes[] =
    {
        // ".img" suffix retry for a path that omits it (Nina's divider).
        { "UI/GuildMark/Mark/Pattern/00004014/11", 15, 15 },
        // QuestIcon moved from UIWindow.img to UIWindow2.img in newer data.
        { "UI/UIWindow.img/QuestIcon/3", 142, 18 },
        // ".img" retry inside the Effect domain (option hearts).
        { "Effect/CharacterEff/1062114/1/0", 11, 10 },
        // Cross-domain: Item.nx and Map.nx resolve through their own roots.
        { "Item/Consume/0200.img/02000000/info/icon", 27, 30 },
        { "Map/Back/chewchewIsland.img/back/0", 190, 11 },
        // Case-insensitive domain names (server scripts are inconsistent).
        { "effect/CharacterEff.img/1062114/1/0", 11, 10 }
    };

    for (const NodeCase& test : nodes)
    {
        check_bitmap(test);
    }

    const char* missing[] =
    {
        "UI/DoesNotExist/1",
        "Bogus/Whatever/0",
        "UI/GuildMark/Mark/Pattern/00004014/11/extra/leaf"
    };

    for (const char* path : missing)
    {
        check(!resolve_embed_node(path),
            std::string("missing: ") + path);
    }

    if (!failures.empty())
    {
        std::cout << failures.size() << " failure(s)\n";
        return 1;
    }

    std::cout << "ALL CHECKS PASSED\n";
    return 0;
}
