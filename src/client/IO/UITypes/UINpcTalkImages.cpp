//////////////////////////////////////////////////////////////////////////////
// Inline-image macro support for NPC dialogue text (see UINpcTalkImages.h).
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkImages.h"

#include "../../Console.h"

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

            // Every top-level nx file doubles as a `#f` domain: `Item/...`
            // resolves in Item.nx, `Map/...` in Map.nx and so on. Matching
            // is case-insensitive because server scripts are inconsistent.
            nl::node domain_root(const std::string& domain)
            {
                struct DomainEntry
                {
                    const char* name;
                    nl::node* root;
                };

                static const DomainEntry DOMAINS[] =
                {
                    { "base", &nl::nx::base },
                    { "character", &nl::nx::character },
                    { "effect", &nl::nx::effect },
                    { "etc", &nl::nx::etc },
                    { "item", &nl::nx::item },
                    { "map", &nl::nx::map },
                    { "mob", &nl::nx::mob },
                    { "morph", &nl::nx::morph },
                    { "npc", &nl::nx::npc },
                    { "quest", &nl::nx::quest },
                    { "reactor", &nl::nx::reactor },
                    { "skill", &nl::nx::skill },
                    { "sound", &nl::nx::sound },
                    { "string", &nl::nx::string },
                    { "tamingmob", &nl::nx::tamingmob },
                    { "ui", &nl::nx::ui }
                };

                std::string lowered;
                lowered.reserve(domain.size());
                for (const char ch : domain)
                {
                    lowered.push_back(
                        static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))
                    );
                }

                for (const DomainEntry& entry : DOMAINS)
                {
                    if (lowered == entry.name)
                    {
                        return *entry.root;
                    }
                }

                return {};
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

            const nl::node root = domain_root(domain);
            if (!root)
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
                if (!resolved)
                {
                    continue;
                }

                if (resolved.data_type() == nl::node::type::bitmap)
                {
                    return resolved;
                }

                // Some images are containers whose frames are canvases
                // (`UIWindow2.img/QuestIcon/3/0`); the dialogue macro always
                // shows the first frame.
                const nl::node frame = resolved["0"];
                if (frame && frame.data_type() == nl::node::type::bitmap)
                {
                    return frame;
                }
            }

            return {};
        }

        std::vector<NpcEmbedSprite> extract_embed_sprites(
            const std::string& source,
            int16_t advance_per_space,
            std::string& out_text)
        {
            out_text.clear();
            out_text.reserve(source.size());

            std::vector<NpcEmbedSprite> sprites;
            advance_per_space = std::max<int16_t>(1, advance_per_space);

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
