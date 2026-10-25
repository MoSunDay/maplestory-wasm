//////////////////////////////////////////////////////////////////////////////
// Whitespace cleanup for NPC dialogue text (see UINpcTalkWhitespace.h).
//////////////////////////////////////////////////////////////////////////////
#include "UINpcTalkWhitespace.h"

#include <cstddef>

namespace jrc
{
    namespace npctalk_text
    {
        namespace
        {
            bool is_ascii_blank(char ch)
            {
                return ch == ' ' || ch == '\t';
            }

            bool line_has_content(const std::string& source, size_t begin, size_t end)
            {
                for (size_t i = begin; i < end; ++i)
                {
                    if (!is_ascii_blank(source[i]))
                    {
                        return true;
                    }
                }

                return false;
            }

            // Append source[begin, end) with blank characters trimmed at both
            // ends and internal runs collapsed to a single space.
            void append_trimmed_line(const std::string& source, size_t begin, size_t end, std::string& out)
            {
                size_t first = begin;
                while (first < end && is_ascii_blank(source[first]))
                {
                    ++first;
                }

                size_t last = end;
                while (last > first && is_ascii_blank(source[last - 1]))
                {
                    --last;
                }

                bool pending_space = false;
                for (size_t i = first; i < last; ++i)
                {
                    const char ch = source[i];
                    if (is_ascii_blank(ch))
                    {
                        pending_space = true;
                        continue;
                    }

                    if (pending_space)
                    {
                        out.push_back(' ');
                        pending_space = false;
                    }

                    out.push_back(ch);
                }
            }
        }

        std::string normalize_dialogue_whitespace(const std::string& source)
        {
            std::string normalized;
            normalized.reserve(source.size());

            bool has_content = false;
            bool pending_blank_line = false;
            size_t cursor = 0;
            while (cursor <= source.size())
            {
                size_t line_end = cursor;
                while (line_end < source.size() &&
                    source[line_end] != '\n' &&
                    source[line_end] != '\r')
                {
                    ++line_end;
                }

                if (line_has_content(source, cursor, line_end))
                {
                    if (has_content)
                    {
                        normalized.push_back('\n');
                        if (pending_blank_line)
                        {
                            normalized.push_back('\n');
                        }
                    }

                    append_trimmed_line(source, cursor, line_end, normalized);
                    has_content = true;
                    pending_blank_line = false;
                }
                else if (has_content)
                {
                    // Remember the blank line; it is only emitted when more
                    // text follows, so trailing blank lines disappear.
                    pending_blank_line = true;
                }

                if (line_end >= source.size())
                {
                    break;
                }

                // CRLF counts as one line break, CR alone as well.
                if (source[line_end] == '\r' &&
                    line_end + 1 < source.size() &&
                    source[line_end + 1] == '\n')
                {
                    cursor = line_end + 2;
                }
                else
                {
                    cursor = line_end + 1;
                }
            }

            return normalized;
        }
    }
}
