// Standalone verifier for the NPC dialogue `#f<path>#` inline-image macro:
// token heuristics and cross-nx-domain path resolution (UINpcTalkImages).
//
// No graphics state is needed: only the parser and `resolve_embed_node` are
// kept by --gc-sections, so Texture/FontCache stay unlinked.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing
//       -Wno-unused-function -ffunction-sections -fdata-sections
//       -Isrc/client -Isrc -Isrc/nlnx
//       tools/verify/npc-embed-path-nx.cpp
//       src/client/IO/UITypes/UINpcTalkImages.cpp
//       src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp src/nlnx/nx.cpp
//       -Wl,--gc-sections -o /tmp/npc-embed-path-nx
//   /tmp/npc-embed-path-nx assets/UI.nx assets/Effect.nx assets/Item.nx assets/Map.nx
//
// Asserts:
//   * `#f...#` recognition accepts path-like bodies and rejects colour-token
//     lookalikes (`#f`, spaces, missing '/', over-long bodies);
//   * UI paths without ".img" and the UIWindow.img->UIWindow2.img QuestIcon
//     relocation still resolve to bitmaps;
//   * every nx domain used by server scripts (UI/Effect/Item/Map here)
//     resolves through its own file and maps onto a bitmap of known size;
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
    using jrc::npctalk_images::resolve_embed_node;
    using jrc::npctalk_images::try_parse_embed_token;

    struct TokenCase
    {
        const char* source;
        size_t cursor;
        bool accepted;
        const char* path;
    };

    struct NodeCase
    {
        const char* path;
        int16_t width;
        int16_t height;
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
