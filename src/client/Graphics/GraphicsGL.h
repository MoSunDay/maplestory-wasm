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
#pragma once
#include "BitmapLoadClass.h"
#include "DrawArgument.h"
#include "FontCache.h"
#include "Text.h"

#include "../Constants.h"
#include "../Error.h"
#include "../Util/QuadTree.h"
#include "../Template/Rectangle.h"
#include "../Template/Singleton.h"

#include "nlnx/bitmap.hpp"

#include "GL/glew.h"

#include "ft2build.h"
#include FT_FREETYPE_H

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jrc
{
    // Graphics engine which uses OpenGL.
    class GraphicsGL : public Singleton<GraphicsGL>
    {
    public:
        using BitmapLoadClass = bitmap_loading::LoadClass;

        struct BitmapBatchProgress
        {
            size_t resident = 0;
            size_t total = 0;
            size_t prepared = 0;
            size_t required = 0;

            bool ready() const
            {
                return resident == total && prepared == required;
            }
        };

        GraphicsGL();

        // Initialise all resources.
        Error init();
        // Re-initialise after changing screen modes.
        void reinit();
        // Update runtime viewport-dependent uniforms and clipping.
        void set_screensize(int16_t width, int16_t height);

        // Clear all bitmaps if most of the space is used up.
        void clear();

        // Add a bitmap to the available resources.
        void addbitmap(const nl::bitmap& bmp);
        // Schedule a bitmap for network fetch and frame-budgeted upload.
        // Visible one-shot effects may promote an already queued bitmap.
        void queuebitmap(
            const nl::bitmap& bmp,
            BitmapLoadClass load_class = BitmapLoadClass::BACKGROUND
        );
        // Prepare ready bitmaps within a soft per-frame time budget.
        void preparebitmaps(uint32_t budget_ms = 2);
        // Track the NX residency of textures created for one map transition.
        uint64_t beginbitmapbatch();
        BitmapBatchProgress bitmapbatchprogress(uint64_t generation) const;
        uint64_t bitmapbatchrevision(uint64_t generation) const;
        void retrybitmapbatch(uint64_t generation);
        void endbitmapbatch(uint64_t generation);
        bool hasblockingbitmaps() const;
        // Return whether a bitmap is currently resident in the texture atlas.
        bool hasbitmap(const nl::bitmap& bmp) const;
        // Return whether a destination rectangle can affect the current viewport.
        bool isonscreen(const Rectangle<int16_t>& rect) const;
        // Draw the bitmap with the given parameters.
        void draw(const nl::bitmap& bmp, const Rectangle<int16_t>& rect,
            const Color& color, float angle);

        // Create a layout for the text with the parameters specified.
        Text::Layout createlayout(const std::string& text, Text::Font font,
            Text::Alignment alignment, int16_t maxwidth, bool formatted);
        // Draw a text with the given parameters.
        void drawtext(const DrawArgument& args, const std::string& text, const Text::Layout& layout, Text::Font font,
            Text::Color color, Text::Background back);

        // Draw a rectangle filled with the specified color.
        void drawrectangle(int16_t x, int16_t y, int16_t w, int16_t h, float r, float g, float b, float a);
        // Fill the screen with the specified color.
        void drawscreenfill(float r, float g, float b, float a);

        // Lock the current scene.
        void lock();
        // Unlock the scene.
        void unlock();

        // Draw the buffer contents with the specified scene opacity.
        void flush(float opacity);
        // Clear the buffer contents.
        void clearscene();

        // Allocate space for a w by h glyph in the font region of the atlas and
        // upload its single-channel bitmap. Fills the region origin on success.
        // Returns false when the atlas cannot fit another glyph.
        bool upload_glyph(GLshort w, GLshort h, GLshort pitch, const uint8_t* bitmap, GLshort& x, GLshort& y);

    private:
        void clearinternal();
        void addtobitmapbatch(const nl::bitmap& bmp, BitmapLoadClass load_class);

        struct Offset
        {
            GLshort l;
            GLshort r;
            GLshort t;
            GLshort b;

            Offset(GLshort x, GLshort y, GLshort w, GLshort h)
            {
                l = x;
                r = x + w;
                t = y ;
                b = y + h;
            }

            Offset()
            {
                l = 0;
                r = 0;
                t = 0;
                b = 0;
            }
        };
        // Add a bitmap to the available resources.
        const Offset& getoffset(const nl::bitmap& bmp);

        struct Leftover
        {
            GLshort l;
            GLshort r;
            GLshort t;
            GLshort b;

            Leftover(GLshort x, GLshort y, GLshort w, GLshort h)
            {
                l = x;
                r = x + w;
                t = y;
                b = y + h;
            }

            Leftover()
            {
                l = 0;
                r = 0;
                t = 0;
                b = 0;
            }

            GLshort width() const
            {
                return r - l;
            }

            GLshort height() const
            {
                return b - t;
            }
        };

        struct Quad
        {
            struct Vertex
            {
                GLshort x;
                GLshort y;
                GLshort s;
                GLshort t;

                Color c;
            };

            static const size_t LENGTH = 4;
            Vertex vertices[LENGTH];

            Quad(GLshort l, GLshort r, GLshort t, GLshort b, const Offset& o,
                const Color& color, GLfloat rot) {

                vertices[0] = { l, t, o.l, o.t, color };
                vertices[1] = { l, b, o.l, o.b, color };
                vertices[2] = { r, b, o.r, o.b, color };
                vertices[3] = { r, t, o.r, o.t, color };

                if (rot != 0.0f)
                {
                    float cos = std::cos(rot);
                    float sin = std::sin(rot);
                    GLshort cx = (l + r) / 2;
                    GLshort cy = (t + b) / 2;

                    for (int i = 0; i < 4; i++)
                    {
                        GLshort vx = vertices[i].x - cx;
                        GLshort vy = vertices[i].y - cy;
                        GLfloat rx = std::roundf(vx * cos - vy * sin);
                        GLfloat ry = std::roundf(vx * sin + vy * cos);
                        vertices[i].x = static_cast<GLshort>(rx + cx);
                        vertices[i].y = static_cast<GLshort>(ry + cy);
                    }
                }
            }
        };

        class LayoutBuilder
        {
        public:
            LayoutBuilder(FontCache& fontcache, Text::Font font, Text::Alignment alignment, int16_t maxwidth, bool formatted);

            size_t add(const char* text, size_t prev, size_t first, size_t last);
            Text::Layout finish(size_t first, size_t last);

        private:
            void add_word(size_t first, size_t last, Text::Font font, Text::Color color);
            void add_line();

            FontCache& fontcache;

            Text::Alignment alignment;
            Text::Font fontid;
            Text::Color color;
            int16_t maxwidth;
            bool formatted;

            int16_t ax;
            int16_t ay;

            std::vector<Text::Layout::Line> lines;
            std::vector<Text::Layout::Word> words;
            std::vector<int16_t> advances;
            int16_t width;
            int16_t endy;
        };

        static Rectangle<int16_t> screen();

        static const GLshort ATLASW = 8192;
        static const GLshort ATLASH = 8192;
        static const GLshort MINLOSIZE = 32;

        bool locked;

        std::vector<Quad> quads;
        GLuint vbo;
        GLuint ibo;
        GLuint atlas;

        GLint program;
        GLint attribute_coord;
        GLint attribute_color;
        GLint uniform_texture;
        GLint uniform_atlassize;
        GLint uniform_screensize;
        GLint uniform_yoffset;
        GLint uniform_fontregion;

        std::unordered_map<size_t, Offset> offsets;
        std::deque<nl::bitmap> pending_priority_bitmaps;
        std::deque<nl::bitmap> pending_bitmaps;
        std::unordered_set<size_t> pending_bitmap_ids;
        std::unordered_set<size_t> pending_blocking_bitmap_ids;
        struct BitmapBatch
        {
            uint64_t generation = 0;
            uint64_t revision = 0;
            std::unordered_map<size_t, nl::bitmap> bitmaps;
            std::unordered_set<size_t> required_bitmap_ids;
        };
        BitmapBatch bitmap_batch;
        uint64_t next_bitmap_batch_generation = 0;
        Offset nulloffset;

        QuadTree<size_t, Leftover> leftovers;
        size_t rlid;
        size_t wasted;
        Point<GLshort> border;
        Range<GLshort> yrange;

        FT_Library ftlibrary;
        // Font glyphs are packed into bands that grow upward from the bottom of
        // the atlas while bitmaps grow downward from the top, so the two
        // allocators only need to check against each other's frontier.
        Point<GLshort> fontborder; // x: band cursor, y: bottom row of the current band
        GLshort fontbandheight;    // tallest glyph in the current band
        GLshort fontregion;        // topmost row used by any glyph (shader boundary)
    };

    //constexpr Rectangle<int16_t> GraphicsGL::SCREEN;
}
