// Text layout and glyph-quad emission for GraphicsGL, split out of
// GraphicsGL.cpp to keep file sizes manageable. Every definition here is a
// member of GraphicsGL declared in GraphicsGL.h; behavior is unchanged.
#include "GraphicsGL.h"

#include "../Util/Utf8.h"

namespace jrc
{
    namespace
    {
        // End of the next lay-outable token starting at offset. Tokens are runs
        // of non-CJK characters up to the next delimiter, except that every CJK
        // codepoint forms its own token: CJK scripts have no spaces, so each
        // ideograph must be an independent line-break opportunity.
        size_t next_token(const char* text, size_t offset, size_t length)
        {
            size_t seqlen = Utf8::sequence_length(text[offset]);
            if (offset + seqlen > length)
            {
                seqlen = length - offset;
            }

            if (Utf8::is_cjk(Utf8::decode(text + offset, length - offset)))
            {
                return offset + seqlen;
            }

            size_t pos = offset + seqlen;
            while (pos < length)
            {
                char c = text[pos];
                if (c == ' ' || c == '\\' || c == '#')
                {
                    return pos;
                }
                if (Utf8::is_cjk(Utf8::decode(text + pos, length - pos)))
                {
                    return pos;
                }
                pos += Utf8::sequence_length(c);
            }
            return length;
        }
    }

    Text::Layout GraphicsGL::createlayout(const std::string& text, Text::Font id,
        Text::Alignment alignment, int16_t maxwidth, bool formatted) {

        size_t length = text.length();
        if (length == 0)
        {
            return {};
        }

        LayoutBuilder builder(FontCache::get(), id, alignment, maxwidth, formatted);

        const char* p_text = text.c_str();

        size_t first = 0;
        size_t offset = 0;
        while (offset < length)
        {
            size_t last = next_token(p_text, offset, length);

            first = builder.add(p_text, first, offset, last);
            offset = last;
        }

        return builder.finish(first, offset);
    }


    GraphicsGL::LayoutBuilder::LayoutBuilder(FontCache& fc, Text::Font f, Text::Alignment a, int16_t mw, bool fm)
        : fontcache(fc), alignment(a), fontid(f), maxwidth(mw), formatted(fm)
    {

        color  = Text::NUM_COLORS;
        ax     = 0;
        ay     = fontcache.linespace(fontid);
        width  = 0;
        endy   = 0;
        if (maxwidth == 0)
        {
            maxwidth = 800;
        }
    }

    size_t GraphicsGL::LayoutBuilder::add(const char* text, size_t prev, size_t first, size_t last)
    {
        if (first == last)
        {
            return prev;
        }

        Text::Font  last_font  = fontid;
        Text::Color last_color = color;
        size_t skip = 0;
        bool linebreak = false;
        if (formatted)
        {
            switch (text[first])
            {
            case '\\':
                if (first + 1 < last)
                {
                    switch (text[first + 1])
                    {
                    case 'n':
                        linebreak = true;
                        break;
                    case 'r':
                        linebreak = ax > 0;
                        break;
                    }
                    skip++;
                }
                skip++;
                break;
            case '#':
                if (first + 1 < last)
                {
                    switch (text[first + 1])
                    {
                    case 'k':
                        color = Text::DARKGREY;
                        break;
                    case 'b':
                        color = Text::BLUE;
                        break;
                    case 'r':
                        color = Text::RED;
                        break;
                    case 'c':
                        color = Text::ORANGE;
                        break;
                    }
                    skip++;
                }
                skip++;
                break;
            }
        }

        int16_t wordwidth = 0;
        if (!linebreak)
        {
            for (size_t i = first; i < last; )
            {
                size_t seqlen = Utf8::sequence_length(text[i]);
                if (i + seqlen > last)
                {
                    seqlen = last - i;
                }
                wordwidth += fontcache.getglyph(fontid, Utf8::decode(text + i, last - i)).ax;

                if (wordwidth > maxwidth)
                {
                    if (i == first)
                    {
                        // A single codepoint already exceeds the limit (only
                        // possible for extremely narrow layouts): emit the
                        // token as-is instead of recursing forever.
                        break;
                    }
                    // Split at the codepoint boundary that overflowed.
                    prev = add(text, prev, first, i);
                    return add(text, prev, i, last);
                }

                i += seqlen;
            }
        }

        bool newword = skip > 0;
        bool newline = linebreak || ax + wordwidth > maxwidth;
        if (newword || newline)
        {
            add_word(prev, first, last_font, last_color);
        }
        if (newline)
        {
            add_line();

            endy = ay;
            ax = 0;
            ay += fontcache.linespace(fontid);
        }

        for (size_t pos = first; pos < last; )
        {
            size_t seqlen = Utf8::sequence_length(text[pos]);
            if (pos + seqlen > last)
            {
                seqlen = last - pos;
            }
            char32_t codepoint = Utf8::decode(text + pos, last - pos);
            const FontCache::Char& ch = fontcache.getglyph(fontid, codepoint);

            // One advance entry per byte keeps all callers byte-indexed;
            // continuation bytes repeat their leader's advance.
            for (size_t byte = 0; byte < seqlen; ++byte)
            {
                advances.push_back(ax);
            }

            bool skipped = pos < first + skip || (newline && codepoint == U' ');
            pos += seqlen;
            if (skipped)
                continue;

            ax += ch.ax;

            if (width < ax)
            {
                width = ax;
            }
        }

        if (newword || newline)
        {
            return first + skip;
        }
        else
        {
            return prev;
        }
    }

    Text::Layout GraphicsGL::LayoutBuilder::finish(size_t first, size_t last)
    {
        add_word(first, last, fontid, color);
        add_line();

        advances.push_back(ax);
        return { lines, advances, width, ay, ax, endy };
    }

    void GraphicsGL::LayoutBuilder::add_word(
        size_t      word_first,
        size_t      word_last,
        Text::Font  word_font,
        Text::Color word_color
    ) {

        words.push_back({ word_first, word_last, word_font, word_color });
    }

    void GraphicsGL::LayoutBuilder::add_line()
    {
        int16_t line_x = 0;
        int16_t line_y = ay;
        switch (alignment)
        {
        case Text::CENTER:
            line_x -= ax / 2;
            break;
        case Text::RIGHT:
            line_x -= ax;
            break;
        default:
            break;
        }

        lines.push_back({ words, { line_x, line_y } });
        words.clear();
    }


    void GraphicsGL::drawtext(const DrawArgument& args, const std::string& text,
        const Text::Layout& layout, Text::Font id, Text::Color colorid, Text::Background background) {

        if (locked)
        {
            return;
        }

        const Color& color = args.get_color();

        if (text.empty() || color.invisible())
        {
            return;
        }

        FontCache& fontcache = FontCache::get();

        GLshort x = args.getpos().x();
        GLshort y = args.getpos().y();
        GLshort w = layout.width();
        GLshort h = layout.height();

        switch (background)
        {
        case Text::NONE:
            break;
        case Text::NAMETAG:
            for (const Text::Layout::Line& line : layout)
            {
                GLshort left = x + line.position.x() - 2;
                GLshort right = left + w + 3;
                GLshort top = y + line.position.y() - fontcache.linespace(id) + 5;
                GLshort bottom = top + h - 2;
                Color ntcolor{ 0.0f, 0.0f, 0.0f, 0.6f };

                quads.emplace_back(left, right, top, bottom, nulloffset, ntcolor, 0.0f);
                quads.emplace_back(left - 1, left, top + 1, bottom - 1, nulloffset, ntcolor, 0.0f);
                quads.emplace_back(right, right + 1, top + 1, bottom - 1, nulloffset, ntcolor, 0.0f);
            }
            break;
        default:
            break;
        }

        constexpr GLfloat colors[Text::NUM_COLORS][3] =
        {
            { 0.0f,  0.0f,  0.0f  }, // Black
            { 1.0f,  1.0f,  1.0f  }, // White
            { 1.0f,  1.0f,  0.0f  }, // Yellow
            { 0.0f,  0.0f,  1.0f  }, // Blue
            { 1.0f,  0.0f,  0.0f  }, // Red
            { 0.8f,  0.3f,  0.3f  }, // DarkRed
            { 0.5f,  0.25f, 0.0f  }, // Brown
            { 0.5f,  0.5f,  0.5f  }, // Lightgrey
            { 0.25f, 0.25f, 0.25f }, // Darkgrey
            { 1.0f,  0.5f,  0.0f  }, // Orange
            { 0.0f,  0.75f, 1.0f  }, // Mediumblue
            { 0.5f,  0.0f,  0.5f  }  // Violet
        };

        for (const Text::Layout::Line& line : layout)
        {
            Point<int16_t> position = line.position;

            for (const Text::Layout::Word& word : line.words)
            {
                GLshort ax = position.x() + layout.advance(word.first);
                GLshort ay = position.y();

                const GLfloat* wordcolor;
                if (word.color < Text::NUM_COLORS)
                {
                    wordcolor = colors[word.color];
                }
                else
                {
                    wordcolor = colors[colorid];
                }
                Color abscolor = color * Color{ wordcolor[0], wordcolor[1], wordcolor[2], 1.0f };

                for (size_t pos = word.first; pos < word.last; )
                {
                    size_t seqlen = Utf8::sequence_length(text[pos]);
                    if (pos + seqlen > word.last)
                    {
                        seqlen = word.last - pos;
                    }
                    char32_t codepoint = Utf8::decode(text.data() + pos, word.last - pos);
                    const FontCache::Char& ch = fontcache.getglyph(id, codepoint);

                    if (ax == 0 && codepoint == U' ')
                    {
                        pos += seqlen;
                        continue;
                    }

                    GLshort chx = x + ax + ch.bl;
                    GLshort chy = y + ay - ch.bt;
                    GLshort chw = ch.bw;
                    GLshort chh = ch.bh;

                    ax += ch.ax;
                    pos += seqlen;

                    if (chw <= 0 || chh <= 0)
                    {
                        continue;
                    }

                    quads.emplace_back(chx, chx + chw, chy, chy + chh, Offset(ch.ox, ch.oy, chw, chh), abscolor, 0.0f);
                }
            }
        }
    }
}
