// Standalone verifier for NPC dialogue whitespace normalization
// (UINpcTalkWhitespace), the pass that runs after macro expansion.
//
// No graphics or nx data is needed: it links only the pure whitespace unit.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client
//       tools/verify/npc-dialog-whitespace.cpp
//       src/client/IO/UITypes/UINpcTalkWhitespace.cpp
//       -o /tmp/npc-dialog-whitespace
//   /tmp/npc-dialog-whitespace
//
// Asserts:
//   * tabs/indentation and repeated spaces collapse to one space;
//   * CRLF, lone CR and LF all normalize to a single line break;
//   * runs of blank lines collapse to exactly one blank line;
//   * leading/trailing blank lines (and surrounding edges) are removed;
//   * UTF-8 bytes and `#..#` token text survive untouched;
//   * empty and whitespace-only input yield an empty string.

#include <iostream>
#include <string>
#include <vector>

#include "IO/UITypes/UINpcTalkWhitespace.h"

namespace
{
    using jrc::npctalk_text::normalize_dialogue_whitespace;

    std::vector<std::string> failures;

    // Render line breaks/tabs visibly so failures stay readable.
    std::string visible(const std::string& text)
    {
        std::string shown;
        for (const char ch : text)
        {
            if (ch == '\n')
            {
                shown += "\\n";
            }
            else if (ch == '\r')
            {
                shown += "\\r";
            }
            else if (ch == '\t')
            {
                shown += "\\t";
            }
            else
            {
                shown.push_back(ch);
            }
        }

        return shown;
    }

    void expect(const std::string& input, const std::string& expected, const char* label)
    {
        const std::string actual = normalize_dialogue_whitespace(input);
        if (actual == expected)
        {
            return;
        }

        failures.push_back(label);
        std::cout << "FAIL  " << label << "\n  input:    \"" << visible(input)
                  << "\"\n  expected: \"" << visible(expected)
                  << "\"\n  actual:   \"" << visible(actual) << "\"\n";
    }
}

int main()
{
    // Tabs, indentation and repeated spaces collapse to single spaces.
    expect("\tIndented   line\twith\t\tgaps   ", "Indented line with gaps",
        "collapse tabs and repeated spaces");
    expect("  a  \t  b  ", "a b", "trim edges and collapse");

    // Every line-break flavour yields one '\n'; single breaks stay intentional.
    expect("first\r\nsecond", "first\nsecond", "crlf normalizes");
    expect("first\rsecond", "first\nsecond", "lone cr normalizes");
    expect("first\nsecond", "first\nsecond", "lf preserved");

    // Blank-line runs collapse to exactly one blank line.
    expect("a\n\n\n\nb", "a\n\nb", "blank line run collapses");
    expect("a\r\n \t \r\n\r\nb", "a\n\nb", "blank lines with whitespace collapse");

    // Leading/trailing blank lines disappear, including padded ones.
    expect("\n\n  \n\na\n\n", "a", "leading and trailing blanks dropped");
    expect("   \n\t\n", "", "whitespace-only empty");

    // UTF-8 payload survives byte for byte; only ASCII blanks are touched.
    expect("\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C\t\xE4\xB8\x96\xE7\x95\x8C",
        "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C \xE4\xB8\x96\xE7\x95\x8C",
        "utf-8 chinese preserved");
    expect("\xE3\x80\x80 \xE3\x80\x80", "\xE3\x80\x80 \xE3\x80\x80",
        "non-ascii spaces untouched");

    // Token bodies (paths/ids) contain no whitespace and are copied verbatim.
    expect("#fUI/GuildMark/Mark/Pattern/00004014/11#   #i4000000#",
        "#fUI/GuildMark/Mark/Pattern/00004014/11# #i4000000#",
        "token text preserved");

    // Empty input stays empty.
    expect("", "", "empty input");

    if (!failures.empty())
    {
        std::cout << failures.size() << " failure(s) in NPC dialogue whitespace\n";
        return 1;
    }

    std::cout << "ALL CHECKS PASSED (npc dialogue whitespace)\n";
    return 0;
}
