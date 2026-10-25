//////////////////////////////////////////////////////////////////////////////
// Inline-image macro support for NPC dialogue text (see UINpcTalkImages.h).
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkImages.h"
#include "UINpcTalkIconData.h"

#include "../../Console.h"

#include "nlnx/nx.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace jrc
{
    namespace npctalk_images
    {
        void embed_display_size(
            int16_t native_width,
            int16_t native_height,
            int16_t max_height,
            int16_t& out_width,
            int16_t& out_height)
        {
            out_width = native_width;
            out_height = native_height;
            if (max_height <= 0 || native_height <= max_height)
            {
                return;
            }

            const int32_t scaled_width =
                (static_cast<int32_t>(native_width) * max_height +
                    native_height / 2) / native_height;
            out_width = static_cast<int16_t>(
                std::max<int32_t>(1, scaled_width)
            );
            out_height = max_height;
        }

        namespace
        {
            constexpr size_t MAX_EMBED_PATH_LENGTH = 120;
            constexpr size_t MAX_UNRESOLVED_LOGS = 20;

            // Text::A12M dialogue line spacing: FontCache bakes 12 px glyphs
            // and uses height * 1.35 + 1, i.e. ~17 px per line. Item and
            // skill icons are natively 32x32, so an option list would overlap
            // neighbouring rows if they kept their native height. Cap those
            // icons to one line; `#f` path embeds stay at native size.
            constexpr int16_t NPC_ICON_MAX_HEIGHT = 17;

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

            // Log each unresolved icon id once instead of once per dialogue.
            void warn_unresolved_icon(NpcIconKind kind, int32_t id)
            {
                static std::unordered_set<std::string> logged;

                const std::string token =
                    std::string(kind == NpcIconKind::ITEM ? "item:" : "skill:") +
                    std::to_string(id);
                if (logged.size() >= MAX_UNRESOLVED_LOGS || !logged.insert(token).second)
                {
                    return;
                }

                Console::get().print("[npc] unresolved dialogue icon: " + token);
            }

            // Append a run of spaces as wide as the on-screen sprite and
            // record it so the caller can draw it on top of the placeholder.
            // `max_height` caps the display height (0 keeps native size).
            // Returns false when the texture or the display size has no
            // width (nothing was appended).
            bool append_sprite(
                const Texture& texture,
                int16_t advance_per_space,
                int16_t max_height,
                std::vector<NpcEmbedSprite>& sprites,
                std::string& out_text)
            {
                if (texture.width() <= 0 || texture.height() <= 0)
                {
                    return false;
                }

                int16_t width = 0;
                int16_t height = 0;
                embed_display_size(
                    texture.width(), texture.height(), max_height, width, height
                );
                if (width <= 0 || height <= 0)
                {
                    return false;
                }

                const size_t placeholder_spaces = static_cast<size_t>(
                    (width + advance_per_space - 1) / advance_per_space
                );

                sprites.push_back({ texture, out_text.size(), 0, width, height });
                out_text.append(placeholder_spaces, ' ');
                return true;
            }

            // Resolve an `#i`/`#v` (item) or `#s` (skill) icon and append its
            // placeholder. Icons are capped to one dialogue line so option
            // lists stay readable. Unresolved ids leave no text behind.
            void append_icon_sprite(
                NpcIconKind kind,
                int32_t id,
                int16_t advance_per_space,
                std::vector<NpcEmbedSprite>& sprites,
                std::string& out_text)
            {
                const Texture* icon = kind == NpcIconKind::ITEM
                    ? item_icon_texture(id)
                    : skill_icon_texture(id);
                if (icon == nullptr ||
                    !append_sprite(
                        *icon,
                        advance_per_space,
                        NPC_ICON_MAX_HEIGHT,
                        sprites,
                        out_text
                    ))
                {
                    warn_unresolved_icon(kind, id);
                }
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

            if (source[cursor] != '#')
            {
                return false;
            }

            // Server markup lists both `#f..` and `#F..` for inline images.
            if (std::tolower(static_cast<unsigned char>(source[cursor + 1])) != 'f')
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

        bool try_parse_icon_token(
            const std::string& source,
            size_t cursor,
            NpcIconKind& kind,
            int32_t& id,
            size_t& out_end)
        {
            if (cursor + 2 >= source.size() || source[cursor] != '#')
            {
                return false;
            }

            switch (std::tolower(static_cast<unsigned char>(source[cursor + 1])))
            {
            case 'i':
            case 'v':
                kind = NpcIconKind::ITEM;
                break;
            case 's':
                kind = NpcIconKind::SKILL;
                break;
            default:
                return false;
            }

            const size_t digits_start = cursor + 2;
            size_t digits_end = digits_start;
            int64_t value = 0;
            while (digits_end < source.size() &&
                std::isdigit(static_cast<unsigned char>(source[digits_end])))
            {
                value = value * 10 + (source[digits_end] - '0');
                if (value > INT32_MAX)
                {
                    return false;
                }

                ++digits_end;
            }

            if (digits_end == digits_start)
            {
                return false;
            }

            // Some scripts pad the closing '#' with a single ':'.
            size_t close = digits_end;
            if (close < source.size() && source[close] == ':')
            {
                ++close;
            }

            if (close >= source.size() || source[close] != '#')
            {
                return false;
            }

            id = static_cast<int32_t>(value);
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

        int16_t embed_sprite_y(
            int16_t line_baseline,
            int16_t native_height,
            int16_t origin_y,
            int16_t display_height)
        {
            if (native_height <= 0)
            {
                return static_cast<int16_t>(line_baseline - display_height);
            }

            // Distance from the texture origin row to the scaled bottom edge,
            // rounded to the nearest pixel so icon rows share the text
            // baseline. Origins sit inside the canvas, so this is normally
            // in [0, display_height]; the signed branch still keeps
            // out-of-range data predictable.
            const int64_t below_origin =
                static_cast<int64_t>(native_height - origin_y) * display_height;
            const int64_t half = native_height / 2;
            const int64_t offset = below_origin >= 0
                ? (below_origin + half) / native_height
                : -((-below_origin + half) / native_height);

            return static_cast<int16_t>(line_baseline - offset);
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
                if (try_parse_embed_token(source, cursor, path, token_end))
                {
                    const nl::node node = resolve_embed_node(path);
                    if (!node)
                    {
                        warn_unresolved(path);
                    }
                    else
                    {
                        const Texture texture(node);
                        append_sprite(
                            texture, advance_per_space, 0, sprites, out_text
                        );
                    }

                    cursor = token_end;
                    continue;
                }

                NpcIconKind kind = NpcIconKind::ITEM;
                int32_t icon_id = 0;
                if (try_parse_icon_token(source, cursor, kind, icon_id, token_end))
                {
                    append_icon_sprite(kind, icon_id, advance_per_space, sprites, out_text);
                    cursor = token_end;
                    continue;
                }

                out_text.push_back(source[cursor]);
                ++cursor;
            }

            return sprites;
        }
    }
}
