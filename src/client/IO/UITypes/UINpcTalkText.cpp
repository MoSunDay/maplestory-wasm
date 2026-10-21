//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkText.h"

#include "../../Gameplay/Stage.h"

#include "UINpcTalkImages.h"
#include "UINpcTalkRefs.h"

#include <cctype>

namespace jrc
{
    namespace npctalk_text
    {
        namespace
        {
            constexpr int8_t SELECTION_DIALOGUE_TYPE = 4;
            constexpr int8_t LEGACY_SELECTION_DIALOGUE_TYPE = 5;

            // Zero-parameter formatting tokens: bold, color switches, etc.
            // These must be handled before number-parsing to avoid greedily
            // consuming content like #e1000#k → stripping "1000".
            bool is_formatting_token(char ch)
            {
                switch (std::tolower(static_cast<unsigned char>(ch)))
                {
                case 'b':  // blue
                case 'd':  // dark/purple
                case 'e':  // bold on
                case 'f':  // dark-grey
                case 'g':  // green
                case 'k':  // black
                case 'n':  // normal
                case 'r':  // red
                    return true;
                default:
                    return false;
                }
            }

            size_t find_next_selection_tag(const std::string& source, size_t start)
            {
                size_t cursor = source.find("#L", start);
                while (cursor != std::string::npos)
                {
                    size_t id_start = cursor + 2;
                    size_t id_end = id_start;
                    while (id_end < source.size() && std::isdigit(static_cast<unsigned char>(source[id_end])))
                    {
                        id_end++;
                    }

                    if (id_end > id_start && id_end < source.size() && source[id_end] == '#')
                    {
                        return cursor;
                    }

                    cursor = source.find("#L", cursor + 2);
                }

                return std::string::npos;
            }

            std::string trim_selection_text(std::string text)
            {
                while (!text.empty())
                {
                    char ch = text.back();
                    if (ch != '\r' && ch != '\n' && ch != ' ' && ch != '\t')
                    {
                        break;
                    }

                    text.pop_back();
                }

                return text;
            }

            bool try_parse_int32(const std::string& token, int32_t& value)
            {
                if (token.empty())
                {
                    return false;
                }

                try
                {
                    value = std::stoi(token);
                }
                catch (...)
                {
                    return false;
                }

                return true;
            }

            bool try_parse_delimited_number(
                const std::string& source,
                size_t number_start,
                size_t& delimiter_pos,
                int32_t& value
            )
            {
                size_t number_end = number_start;
                while (number_end < source.size() && std::isdigit(static_cast<unsigned char>(source[number_end])))
                {
                    number_end++;
                }

                if (number_end == number_start || number_end >= source.size() || source[number_end] != '#')
                {
                    return false;
                }

                if (!try_parse_int32(source.substr(number_start, number_end - number_start), value))
                {
                    return false;
                }

                delimiter_pos = number_end;
                return true;
            }

        }

        bool is_selection_dialogue_type(int8_t msgtype)
        {
            // Cosmic-compatible servers have been observed using both values
            // for menu prompts with #L...#l options.
            return msgtype == SELECTION_DIALOGUE_TYPE || msgtype == LEGACY_SELECTION_DIALOGUE_TYPE;
        }

        std::string strip_npc_tokens(const std::string& source, EmbedImages embed_images)
        {
            std::string stripped;
            stripped.reserve(source.size());

            size_t cursor = 0;
            while (cursor < source.size())
            {
                if (source[cursor] != '#' || cursor + 1 >= source.size())
                {
                    stripped.push_back(source[cursor]);
                    cursor++;
                    continue;
                }

                char token = source[cursor + 1];

                // Inline-image tokens must be consumed as a whole: their path
                // must never leak into the readable text, and in KEEP mode
                // they survive verbatim for the sprite extraction pass.
                std::string embed_path;
                size_t embed_end = 0;
                if (npctalk_images::try_parse_embed_token(source, cursor, embed_path, embed_end))
                {
                    if (embed_images == EmbedImages::KEEP)
                    {
                        stripped.append(source, cursor, embed_end - cursor);
                    }
                    cursor = embed_end;
                    continue;
                }

                if (token == '#')
                {
                    stripped.push_back('#');
                    cursor += 2;
                    continue;
                }

                if (is_formatting_token(token))
                {
                    cursor += 2;
                    continue;
                }

                if (std::isalpha(static_cast<unsigned char>(token)))
                {
                    int32_t ignored_value = 0;
                    size_t token_end = 0;
                    if (try_parse_delimited_number(source, cursor + 2, token_end, ignored_value))
                    {
                        cursor = token_end + 1;
                        continue;
                    }

                    if (token == 'h' || token == 'H')
                    {
                        size_t token_end_h = source.find('#', cursor + 2);
                        if (token_end_h != std::string::npos)
                        {
                            cursor = token_end_h + 1;
                            continue;
                        }
                    }

                    cursor += 2;
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(token)))
                {
                    int32_t ignored_value = 0;
                    size_t token_end = 0;
                    if (try_parse_delimited_number(source, cursor + 1, token_end, ignored_value))
                    {
                        cursor = token_end + 1;
                        continue;
                    }
                }

                stripped.push_back(source[cursor]);
                cursor++;
            }

            return stripped;
        }

        std::string replace_macros(const std::string& source, EmbedImages embed_images)
        {
            std::string result;
            result.reserve(source.size());

            size_t cursor = 0;
            while (cursor < source.size())
            {
                if (source[cursor] != '#' || cursor + 1 >= source.size())
                {
                    result.push_back(source[cursor]);
                    cursor++;
                    continue;
                }

                char token = source[cursor + 1];

                // Inline-image tokens pass through untouched in KEEP mode (or
                // vanish in STRIP mode); their body is not scanned for macros
                // or formatting letters like the legacy 'f' colour token.
                std::string embed_path;
                size_t embed_end = 0;
                if (npctalk_images::try_parse_embed_token(source, cursor, embed_path, embed_end))
                {
                    if (embed_images == EmbedImages::KEEP)
                    {
                        result.append(source, cursor, embed_end - cursor);
                    }
                    cursor = embed_end;
                    continue;
                }

                // Skip zero-parameter formatting tokens (#b, #e, #k, #n, #r, etc.)
                if (is_formatting_token(token))
                {
                    cursor += 2;
                    continue;
                }

                if (token == 'h' || token == 'H')
                {
                    size_t token_end = source.find('#', cursor + 2);
                    if (token_end != std::string::npos)
                    {
                        result += Stage::get().get_player().get_stats().get_name();
                        cursor = token_end + 1;
                        continue;
                    }
                }

                if (token == 'm' || token == 'M' ||
                    token == 't' || token == 'T' ||
                    token == 'z' || token == 'Z' ||
                    token == 'p' || token == 'P' ||
                    token == 'o' || token == 'O')
                {
                    int32_t id = 0;
                    size_t token_end = 0;
                    if (try_parse_delimited_number(source, cursor + 2, token_end, id))
                    {
                        std::string replacement = resolve_prefixed_reference(token, id);
                        std::string token_text = source.substr(cursor, token_end + 1 - cursor);
                        if (!replacement.empty())
                        {
                            result += replacement;
                        }
                        else
                        {
                            log_unresolved_npc_token(token_text);
                            result += token_text;
                        }

                        cursor = token_end + 1;
                        continue;
                    }
                }

                if (std::isdigit(static_cast<unsigned char>(token)))
                {
                    int32_t id = 0;
                    size_t token_end = 0;
                    if (try_parse_delimited_number(source, cursor + 1, token_end, id))
                    {
                        std::string replacement = resolve_numeric_reference(id);
                        std::string token_text = source.substr(cursor, token_end + 1 - cursor);
                        if (!replacement.empty())
                        {
                            result += replacement;
                        }
                        else
                        {
                            log_unresolved_npc_token(token_text);
                            result += token_text;
                        }

                        cursor = token_end + 1;
                        continue;
                    }
                }

                result.push_back(source[cursor]);
                cursor++;
            }

            return result;
        }

        SelectionList parse_selections(const std::string& source, EmbedImages embed_images)
        {
            SelectionList result;

            size_t cursor = 0;
            while (cursor < source.size())
            {
                size_t begin = source.find("#L", cursor);
                if (begin == std::string::npos)
                {
                    result.rendered_text += source.substr(cursor);
                    break;
                }

                result.rendered_text += source.substr(cursor, begin - cursor);

                size_t id_start = begin + 2;
                size_t id_end = id_start;
                while (id_end < source.size() && std::isdigit(static_cast<unsigned char>(source[id_end])))
                    id_end++;

                if (id_end >= source.size() || source[id_end] != '#')
                {
                    result.rendered_text += source.substr(begin);
                    break;
                }

                size_t option_start = id_end + 1;
                size_t option_end = source.find("#l", option_start);
                bool has_explicit_end = option_end != std::string::npos;
                if (!has_explicit_end)
                {
                    option_end = find_next_selection_tag(source, option_start);
                    if (option_end == std::string::npos)
                    {
                        option_end = source.size();
                    }
                }

                if (id_end == id_start)
                {
                    result.rendered_text += source.substr(begin, option_end - begin);
                    cursor = has_explicit_end ? option_end + 2 : option_end;
                    continue;
                }

                int32_t selection_id = 0;
                if (!try_parse_int32(source.substr(id_start, id_end - id_start), selection_id))
                {
                    result.rendered_text += source.substr(begin, option_end - begin);
                    cursor = has_explicit_end ? option_end + 2 : option_end;
                    continue;
                }

                result.option_ids.push_back(selection_id);
                result.option_texts.push_back(
                    trim_selection_text(
                        strip_npc_tokens(
                            source.substr(option_start, option_end - option_start),
                            embed_images
                        )
                    )
                );
                cursor = has_explicit_end ? option_end + 2 : option_end;
            }

            result.rendered_text = strip_npc_tokens(result.rendered_text, embed_images);
            return result;
        }
    }
}
