//////////////////////////////////////////////////////////////////////////////
// Whitespace cleanup for NPC dialogue text.
//
// Server scripts are authored in windows editors: they mix CRLF/CR/LF line
// breaks, indent every line with tabs and pad labels with runs of spaces.
// Drawing that verbatim leaves ragged indents and stray empty lines in the
// dialogue window. This module is a pure post-processing pass applied after
// macro expansion; it never inspects or rewrites `#..#` token bodies (paths
// and ids contain no whitespace anyway).
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

namespace jrc
{
    namespace npctalk_text
    {
        // Normalize dialogue whitespace:
        //   * CRLF, lone CR and LF all become a single '\n';
        //   * ASCII spaces/tabs are trimmed at both ends of every line;
        //   * internal runs of ASCII spaces/tabs collapse to one space;
        //   * leading/trailing blank lines are dropped and every run of blank
        //     lines collapses to exactly one blank line ("\n\n");
        //   * all other bytes (UTF-8 text, '#' tokens) are preserved.
        // Returns an empty string for empty or whitespace-only input.
        std::string normalize_dialogue_whitespace(const std::string& source);
    }
}
