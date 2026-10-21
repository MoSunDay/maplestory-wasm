//////////////////////////////////////////////////////////////////////////////
// Inline-image macro support for NPC dialogue text (see UINpcTalkImages.h).
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkImages.h"

#include "../../Console.h"
#include "../../Graphics/FontCache.h"
#include "../../Graphics/Text.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace jrc
{
    namespace npctalk_images
    {
        namespace
        {
            constexpr size_t MAX_EMBED_PATH_LENGTH = 120;
            constexpr size_t MAX_UNRESOLVED_LOGS = 20;

            bool ends_with_img(const std::string& segment)
            {
                return segment.size() >= 4 &&
                    segment.compare(segment.size() - 4, 4, ".img") == 0;
            }

            // Retry the first path segment with an explicit ".img" suffix,
            // e.g. `GuildMark/Mark/...` -> `GuildMark.img/Mark/...`.
            std::string with_img_suffix(const std::string& rest)
            {
                const size_t slash = rest.find('/');
                const std::string head =
                    slash == std::string::npos ? rest : rest.substr(0, slash);
                const std::string tail =
                    slash == std::string::npos ? std::string() : rest.substr(slash);

                return ends_with_img(head) ? rest : head + ".img" + tail;
            }

            // Newer data files moved `UIWindow.img/QuestIcon/...` to
            // `UIWindow2.img/QuestIcon/...`; mirror that move on a miss.
            std::string with_uiwindow2(const std::string& rest)
            {
                const size_t slash = rest.find('/');
                const std::string head =
                    slash == std::string::npos ? rest : rest.substr(0, slash);

                if (head != "UIWindow" && head != "UIWindow.img")
                {
                    return rest;
                }

                const std::string tail =
                    slash == std::string::npos ? std::string() : rest.substr(slash);
                return "UIWindow2.img" + tail;
            }

            // Log each unresolved path once instead of once per dialogue.
            void warn_unresolved(const std::string& path)
            {
                static std::unordered_set<std::string> logged;

                if (logged.size() >= MAX_UNRESOLVED_LOGS || !logged.insert(path).second)
                {
                    return;
                }

                Console::get().print("[npc] unresolved inline image: " + path);
            }

            int16_t space_advance()
            {
                const FontCache::Char& space =
                    FontCache::get().getglyph(Text::A12M, U' ');
                return std::max<int16_t>(1, space.ax);
            }
        }

        bool try_parse_embed_token(
            const std::string& source,
            size_t cursor,
            std::string& out_path,
            size_t& out_end)
        {
            if (cursor + 1 >= source.size())
            {
                return false;
            }

            if (source[cursor] != '#' || source[cursor + 1] != 'f')
            {
                return false;
            }

            const size_t body_start = cursor + 2;
            const size_t close = source.find('#', body_start);
            if (close == std::string::npos)
            {
                return false;
            }

            const size_t length = close - body_start;
            if (length == 0 || length > MAX_EMBED_PATH_LENGTH)
            {
                return false;
            }

            if (source.find('/', body_start) >= close)
            {
                return false;
            }

            for (size_t i = body_start; i < close; ++i)
            {
                if (std::isspace(static_cast<unsigned char>(source[i])))
                {
                    return false;
                }
            }

            out_path = source.substr(body_start, length);
            out_end = close + 1;
            return true;
        }

        nl::node resolve_embed_node(const std::string& path)
        {
            const size_t slash = path.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 >= path.size())
            {
                return {};
            }

            const std::string domain = path.substr(0, slash);
            const std::string rest = path.substr(slash + 1);

            nl::node root;
            if (domain == "UI" || domain == "ui")
            {
                root = nl::nx::ui;
            }
            else if (domain == "Effect" || domain == "effect")
            {
                root = nl::nx::effect;
            }
            else
            {
                return {};
            }

            const std::string candidates[3] =
            {
                rest,
                with_img_suffix(rest),
                with_uiwindow2(rest)
            };

            for (const std::string& candidate : candidates)
            {
                const nl::node resolved = root.resolve(candidate);
                if (resolved && resolved.data_type() == nl::node::type::bitmap)
                {
                    return resolved;
                }
            }

            return {};
        }

        std::vector<NpcEmbedSprite> extract_embed_sprites(
            const std::string& source,
            std::string& out_text)
        {
            out_text.clear();
            out_text.reserve(source.size());

            std::vector<NpcEmbedSprite> sprites;
            const int16_t advance_per_space = space_advance();

            size_t cursor = 0;
            while (cursor < source.size())
            {
                std::string path;
                size_t token_end = 0;
                if (!try_parse_embed_token(source, cursor, path, token_end))
                {
                    out_text.push_back(source[cursor]);
                    ++cursor;
                    continue;
                }

                const nl::node node = resolve_embed_node(path);
                if (!node)
                {
                    warn_unresolved(path);
                }
                else
                {
                    const Texture texture(node);
                    if (texture.width() > 0)
                    {
                        const size_t placeholder_spaces = static_cast<size_t>(
                            (texture.width() + advance_per_space - 1) / advance_per_space
                        );

                        sprites.push_back({ texture, out_text.size(), 0 });
                        out_text.append(placeholder_spaces, ' ');
                    }
                }

                cursor = token_end;
            }

            return sprites;
        }
    }
}
