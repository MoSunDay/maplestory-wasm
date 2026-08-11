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
#include "GraphicsGL.h"

#include "../Configuration.h"
#include "../Console.h"
#include "../Util/Utf8.h"

#include <algorithm>
#include <vector>

namespace jrc
{
    GraphicsGL::GraphicsGL()
    {
        locked = false;
    }

    Error GraphicsGL::init()
    {
        if (glewInit())
        {
            return Error::GLEW;
        }

        if (FT_Init_FreeType(&ftlibrary))
        {
            return Error::FREETYPE;
        }

        FontCache::get().init(ftlibrary);

        GLint result = GL_FALSE;

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        const char *vs_source =
            "precision highp float;\n"
            "attribute vec4 coord;"
            "attribute vec4 color;"
            "varying vec2 texpos;"
            "varying vec4 colormod;"
            "uniform vec2 screensize;"
            "uniform int yoffset;"

            "void main(void) {"
            "    float x = -1.0 + coord.x * 2.0 / screensize.x;"
            "    float y = 1.0 - (coord.y + float(yoffset)) * 2.0 / screensize.y;"
            "    gl_Position = vec4(x, y, 0.0, 1.0);"
            "    texpos = coord.zw;"
            "    colormod = color;"
            "}";
        glShaderSource(vs, 1, &vs_source, NULL);
        glCompileShader(vs);
        glGetShaderiv(vs, GL_COMPILE_STATUS, &result);
        if (!result)
        {
            char infoLog[512];
            glGetShaderInfoLog(vs, 512, NULL, infoLog);
            Console::get().print("Vertex shader compilation failed: " + std::string(infoLog));
            return Error::VERTEX_SHADER;
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        const char *fs_source =
            "precision mediump float;\n"
            "varying vec2 texpos;"
            "varying vec4 colormod;"
            "uniform sampler2D texture;"
            "uniform vec2 atlassize;"
            "uniform int fontregion;"

            "void main(void) {"
            "    if (texpos.y == 0.0) {"
            "        gl_FragColor = colormod;"
            "    } else if (texpos.y >= float(fontregion)) {"
            "        gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(texture, texpos / atlassize).r) * colormod;"
            "    } else {"
            "        gl_FragColor = texture2D(texture, texpos / atlassize) * colormod;"
            "    }"
            "}";
        glShaderSource(fs, 1, &fs_source, NULL);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &result);
        if (!result)
        {
            char infoLog[512];
            glGetShaderInfoLog(fs, 512, NULL, infoLog);
            Console::get().print("Fragment shader compilation failed: " + std::string(infoLog));
            return Error::FRAGMENT_SHADER;
        }

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &result);
        if (!result)
        {
            return Error::SHADER_PROGRAM;
        }

        attribute_coord    = glGetAttribLocation(program, "coord");
        attribute_color    = glGetAttribLocation(program, "color");
        uniform_texture    = glGetUniformLocation(program, "texture");
        uniform_atlassize  = glGetUniformLocation(program, "atlassize");
        uniform_screensize = glGetUniformLocation(program, "screensize");
        uniform_yoffset    = glGetUniformLocation(program, "yoffset");
        uniform_fontregion = glGetUniformLocation(program, "fontregion");
        if (
            attribute_coord    == -1 ||
            attribute_color    == -1 ||
            uniform_texture    == -1 ||
            uniform_atlassize  == -1 ||
            uniform_yoffset    == -1 ||
            uniform_screensize == -1
        ) {
            return Error::SHADER_VARS;
        }

        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, ATLASW, ATLASH, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr
        );

        fontborder.set_y(ATLASH);
        fontbandheight = 0;
        fontregion = ATLASH;

        std::string font_normal = Setting<FontPathNormal>().get().load();
        std::string font_bold = Setting<FontPathBold>().get().load();
        if (font_normal.empty() || font_bold.empty())
        {
            Console::get().print(
                "Warning: A font path is empty, check your settings file."
            );
        }

        constexpr const char* FALLBACK_FONT_NORMAL = "/fonts/Arial/Arial.ttf";
        constexpr const char* FALLBACK_FONT_BOLD = "/fonts/Arial/Arial-Bold.ttf";

        if (font_normal.empty())
        {
            font_normal = FALLBACK_FONT_NORMAL;
        }
        if (font_bold.empty())
        {
            font_bold = FALLBACK_FONT_BOLD;
        }

        auto load_font = [&](const std::string& configured_path,
                             const char* fallback_path,
                             Text::Font id,
                             FT_UInt pixelw,
                             FT_UInt pixelh) {
            if (FontCache::get().addfont(configured_path.c_str(), id, pixelw, pixelh))
            {
                return;
            }

            if (configured_path != fallback_path &&
                FontCache::get().addfont(fallback_path, id, pixelw, pixelh))
            {
                Console::get().print(
                    "Failed to load font '" + configured_path +
                    "', using fallback '" + std::string(fallback_path) + "'."
                );
                return;
            }

            Console::get().print(
                "Failed to load font '" + configured_path +
                "' and fallback '" + std::string(fallback_path) + "'."
            );
        };

        load_font(font_normal, FALLBACK_FONT_NORMAL, Text::A11L, 0, 11);
        load_font(font_normal, FALLBACK_FONT_NORMAL, Text::A11M, 0, 11);
        load_font(font_bold,   FALLBACK_FONT_BOLD,   Text::A11B, 0, 11);
        load_font(font_normal, FALLBACK_FONT_NORMAL, Text::A12M, 0, 12);
        load_font(font_bold,   FALLBACK_FONT_BOLD,   Text::A12B, 0, 12);
        load_font(font_normal, FALLBACK_FONT_NORMAL, Text::A13M, 0, 13);
        load_font(font_bold,   FALLBACK_FONT_BOLD,   Text::A13B, 0, 13);
        load_font(font_normal, FALLBACK_FONT_NORMAL, Text::A18M, 0, 18);

        leftovers = QuadTree<size_t, Leftover>([](const Leftover& first, const Leftover& second) {
            bool wcomp = first.width() >= second.width();
            bool hcomp = first.height() >= second.height();
            if (wcomp && hcomp)
            {
                return QuadTree<size_t, Leftover>::RIGHT;
            }
            else if (wcomp)
            {
                return QuadTree<size_t, Leftover>::DOWN;
            }
            else if (hcomp)
            {
                return QuadTree<size_t, Leftover>::UP;
            }
            else
            {
                return QuadTree<size_t, Leftover>::LEFT;
            }
        });

        return Error::NONE;
    }

    bool GraphicsGL::upload_glyph(GLshort w, GLshort h, GLshort pitch, const uint8_t* bitmap, GLshort& x, GLshort& y)
    {
        if (w <= 0 || h <= 0)
        {
            return false;
        }

        if (fontborder.x() + w > ATLASW)
        {
            // The current band is full: open a new one above it.
            fontborder.set_x(0);
            fontborder.set_y(fontborder.y() - fontbandheight);
            fontbandheight = 0;
        }

        // Glyphs are anchored to the band bottom so that the band's top edge
        // can still grow when a taller glyph joins the band later.
        x = fontborder.x();
        y = fontborder.y() - h;

        // Do not claim rows the bitmap allocator may still hand out.
        if (y < border.y() + yrange.second())
        {
            return false;
        }

        fontborder.shift_x(w);
        if (h > fontbandheight)
        {
            fontbandheight = h;
        }

        if (y < fontregion)
        {
            fontregion = y;
            glUseProgram(program);
            glUniform1i(uniform_fontregion, fontregion);
        }

        glBindTexture(GL_TEXTURE_2D, atlas);

#ifdef MS_PLATFORM_WASM
        // WebGL path: expand the single-channel glyph bitmap to RGBA.
        std::vector<uint8_t> rgba_buffer(static_cast<size_t>(w) * h * 4);
        for (GLshort row = 0; row < h; ++row)
        {
            const uint8_t* src = bitmap + static_cast<size_t>(row) * pitch;
            uint8_t* dst = rgba_buffer.data() + static_cast<size_t>(row) * w * 4;
            for (GLshort col = 0; col < w; ++col)
            {
                uint8_t val = src[col];
                dst[static_cast<size_t>(col) * 4 + 0] = val;
                dst[static_cast<size_t>(col) * 4 + 1] = val;
                dst[static_cast<size_t>(col) * 4 + 2] = val;
                dst[static_cast<size_t>(col) * 4 + 3] = val;
            }
        }
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, x, y, w, h,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba_buffer.data()
        );
#else
        if (pitch == w)
        {
            glTexSubImage2D(
                GL_TEXTURE_2D, 0, x, y, w, h,
                GL_RED, GL_UNSIGNED_BYTE, bitmap
            );
        }
        else
        {
            // FreeType rows may be padded, upload them one at a time.
            for (GLshort row = 0; row < h; ++row)
            {
                glTexSubImage2D(
                    GL_TEXTURE_2D, 0, x, y + row, w, 1,
                    GL_RED, GL_UNSIGNED_BYTE, bitmap + static_cast<size_t>(row) * pitch
                );
            }
        }
#endif

        return true;
    }

    void GraphicsGL::reinit()
    {
        glUseProgram(program);

        glUniform1i(uniform_yoffset, Constants::VIEWYOFFSET);
        glUniform1i(uniform_fontregion, fontregion);
        glUniform2f(uniform_atlassize, ATLASW, ATLASH);
        glUniform2f(uniform_screensize, Constants::viewwidth(), Constants::viewheight());

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(attribute_coord, 4, GL_SHORT, GL_FALSE, sizeof(Quad::Vertex), 0);
        glVertexAttribPointer(attribute_color, 4, GL_FLOAT, GL_FALSE, sizeof(Quad::Vertex), (const void*)8);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindTexture(GL_TEXTURE_2D, atlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        clearinternal();
    }

    void GraphicsGL::set_screensize(int16_t width, int16_t height)
    {
        glUseProgram(program);
        glUniform2f(uniform_screensize, width, height);
    }

    Rectangle<int16_t> GraphicsGL::screen()
    {
        return {
            0,
            Constants::viewwidth(),
            -Constants::VIEWYOFFSET,
            static_cast<int16_t>(-Constants::VIEWYOFFSET + Constants::viewheight())
        };
    }

    void GraphicsGL::clearinternal()
    {
        border = Point<GLshort>(0, 1);
        yrange = Range<GLshort>();

        offsets.clear();
        leftovers.clear();
        rlid   = 1;
        wasted = 0;
    }

    void GraphicsGL::clear()
    {
        size_t used = ATLASW * border.y() + border.x() * yrange.second();
        double usedpercent = static_cast<double>(used) / (ATLASW * fontregion);
        if (usedpercent > 80.0)
        {
            clearinternal();
        }
    }

    void GraphicsGL::addbitmap(const nl::bitmap& bmp)
    {
        getoffset(bmp);
    }

    const GraphicsGL::Offset& GraphicsGL::getoffset(const nl::bitmap& bmp)
    {
        size_t id = bmp.id();
        auto offiter = offsets.find(id);
        if (offiter != offsets.end())
        {
            return offiter->second;
        }

        GLshort x = 0;
        GLshort y = 0;
        GLshort w = bmp.width();
        GLshort h = bmp.height();

        if (w <= 0 || h <= 0)
        {
            return nulloffset;
        }

        if (!bmp.data())
        {
            return nulloffset;
        }

        auto value = Leftover(x, y, w, h);
        size_t lid = leftovers.findnode(value, [](const Leftover& val, const Leftover& leaf){
            return val.width() <= leaf.width() && val.height() <= leaf.height();
        });

        if (lid > 0)
        {
            const Leftover& leftover = leftovers[lid];

            x = leftover.l;
            y = leftover.t;

            GLshort wdelta = leftover.width() - w;
            GLshort hdelta = leftover.height() - h;

            leftovers.erase(lid);

            wasted -= w * h;

            if (wdelta >= MINLOSIZE && hdelta >= MINLOSIZE)
            {
                leftovers.add(rlid, Leftover(x + w, y + h, wdelta, hdelta));
                rlid++;

                if (w >= MINLOSIZE)
                {
                    leftovers.add(rlid, Leftover(x, y + h, w, hdelta));
                    rlid++;
                }

                if (h >= MINLOSIZE)
                {
                    leftovers.add(rlid, Leftover(x + w, y, wdelta, h));
                    rlid++;
                }
            }
            else if (wdelta >= MINLOSIZE)
            {
                leftovers.add(rlid, Leftover(x + w, y, wdelta, h + hdelta));
                rlid++;
            }
            else if (hdelta >= MINLOSIZE)
            {
                leftovers.add(rlid, Leftover(x, y + h, w + wdelta, hdelta));
                rlid++;
            }
        }
        else
        {
            if (border.x() + w > ATLASW)
            {
                border.set_x(0);
                border.shift_y(yrange.second());
                if (border.y() + h > fontregion)
                {
                    clearinternal();
                }
                else
                {
                    yrange = Range<GLshort>();
                }
            }
            x = border.x();
            y = border.y();

            border.shift_x(w);

            if (h > yrange.second())
            {
                if (x >= MINLOSIZE && h - yrange.second() >= MINLOSIZE)
                {
                    leftovers.add(rlid, Leftover(0, yrange.first(), x, h - yrange.second()));
                    rlid++;
                }

                wasted += x * (h - yrange.second());

                yrange = { y + h, h };
            }
            else if (h < yrange.first() - y)
            {
                if (w >= MINLOSIZE && yrange.first() - y - h >= MINLOSIZE)
                {
                    leftovers.add(rlid, Leftover(x, y + h, w, yrange.first() - y - h));
                    rlid++;
                }

                wasted += w * (yrange.first() - y - h);
            }
        }

        /*
        size_t used = ATLASW * border.y() + border.x() * yrange.second();
        double usedpercent = static_cast<double>(used) / (ATLASW * fontregion);
        double wastedpercent = static_cast<double>(wasted) / used;
        Console::get().print("Used: " + std::to_string(usedpercent) + ", wasted: " + std::to_string(wastedpercent));
        */

#ifdef MS_PLATFORM_WASM
        // WebGL 2 does not support GL_BGRA, so we need to manually swap the channels
        // from BGRA (what the game uses) to RGBA (what WebGL expects).
        int32_t len = w * h * 4;
        std::vector<uint8_t> rgba_buffer(len);
        const uint8_t* src = (const uint8_t*)bmp.data();
        for (int i = 0; i < len; i += 4)
        {
            rgba_buffer[i] = src[i + 2];     // R = B
            rgba_buffer[i + 1] = src[i + 1]; // G = G
            rgba_buffer[i + 2] = src[i];     // B = R
            rgba_buffer[i + 3] = src[i + 3]; // A = A
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba_buffer.data());
#else
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA, GL_UNSIGNED_BYTE, bmp.data());
#endif
        return offsets.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(x, y, w, h)
        ).first->second;
    }

    void GraphicsGL::draw(const nl::bitmap& bmp, const Rectangle<int16_t>& rect,
        const Color& color, float angle)
    {
        if (locked)
        {
            return;
        }

        if (color.invisible())
        {
            return;
        }

        if (!rect.overlaps(screen()))
        {
            return;
        }

        quads.emplace_back(rect.l(), rect.r(), rect.t(), rect.b(), getoffset(bmp), color, angle);
    }

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

    void GraphicsGL::drawrectangle(int16_t x, int16_t y, int16_t w, int16_t h, float r, float g, float b, float a)
    {
        if (locked)
        {
            return;
        }

        quads.emplace_back(x, x + w, y, y + h, nulloffset, Color{ r, g, b, a }, 0.0f);
    }

    void GraphicsGL::drawscreenfill(float r, float g, float b, float a)
    {
        drawrectangle(0, -Constants::VIEWYOFFSET, Constants::viewwidth(), Constants::viewheight(), r, g, b, a);
    }

    void GraphicsGL::lock()
    {
        locked = true;
    }

    void GraphicsGL::unlock()
    {
        locked = false;
    }

    void GraphicsGL::flush(float opacity)
    {
        bool coverscene = opacity != 1.0f;
        if (coverscene)
        {
            float complement = 1.0f - opacity;
            Color color{ 0.0f, 0.0f, 0.0f, complement };
            Rectangle<int16_t> screen_rect = screen();
            quads.emplace_back(screen_rect.l(), screen_rect.r(), screen_rect.t(), screen_rect.b(), nulloffset, color, 0.0f);
        }

        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        GLsizei csize = static_cast<GLsizei>(quads.size() * sizeof(Quad));
        glEnableVertexAttribArray(attribute_coord);
        glEnableVertexAttribArray(attribute_color);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, csize, quads.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(attribute_coord, 4, GL_SHORT, GL_FALSE, sizeof(Quad::Vertex), 0);
        glVertexAttribPointer(attribute_color, 4, GL_FLOAT, GL_FALSE, sizeof(Quad::Vertex), (const void*)8);
#ifdef MS_PLATFORM_WASM
        // Initialize indices for GL_TRIANGLES (2 triangles per quad, 6 indices per quad)
        std::vector<GLushort> indices;
        indices.reserve(quads.size() * 6);
        for (size_t i = 0; i < quads.size(); ++i)
        {
            GLushort base = static_cast<GLushort>(i * 4);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_SHORT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#else
        GLsizei fsize = static_cast<GLsizei>(quads.size() * Quad::LENGTH);
        glDrawArrays(GL_QUADS, 0, fsize);
#endif

        glDisableVertexAttribArray(attribute_coord);
        glDisableVertexAttribArray(attribute_color);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (coverscene)
        {
            quads.pop_back();
        }

    }

    void GraphicsGL::clearscene()
    {
        if (!locked)
        {
            quads.clear();
        }
    }
}
