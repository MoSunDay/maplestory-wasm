#include "FontCache.h"

#include "GraphicsGL.h"

#include "../Console.h"

#include <string>

namespace jrc
{
    namespace
    {
        // Bundled CJK fallback (Apache-2.0, see fonts/DroidSansFallback).
        constexpr const char* FALLBACK_FONT_PATH = "/fonts/DroidSansFallback/DroidSansFallbackFull.ttf";
    }

    void FontCache::init(FT_Library library)
    {
        ftlibrary = library;

        if (FT_New_Face(ftlibrary, FALLBACK_FONT_PATH, 0, &fallbackface))
        {
            fallbackface = nullptr;
            Console::get().print(
                "Warning: failed to load fallback font '" + std::string(FALLBACK_FONT_PATH) +
                "', non-Latin text will not render."
            );
        }
    }

    bool FontCache::addfont(const char* name, Text::Font id, FT_UInt pixelw, FT_UInt pixelh)
    {
        FT_Face face;
        if (FT_New_Face(ftlibrary, name, 0, &face))
        {
            return false;
        }

        if (FT_Set_Pixel_Sizes(face, pixelw, pixelh))
        {
            FT_Done_Face(face);
            return false;
        }

        FontEntry& entry = fonts[id];
        entry.face = face;
        entry.pixelw = pixelw;
        entry.pixelh = pixelh;

        // Bake printable ASCII eagerly: common text hits warm entries and the
        // maximum ASCII height defines the legacy line spacing metric.
        int16_t maxheight = 0;
        for (char32_t c = 32; c < 128; ++c)
        {
            const Char& ch = getglyph(id, c);
            if (ch.bh > maxheight)
            {
                maxheight = ch.bh;
            }
        }
        entry.height = maxheight;

        return true;
    }

    int16_t FontCache::linespace(Text::Font id) const
    {
        return static_cast<int16_t>(fonts[id].height * 1.35 + 1);
    }

    const FontCache::Char& FontCache::getglyph(Text::Font id, char32_t codepoint)
    {
        FontEntry& entry = fonts[id];

        auto iter = entry.chars.find(codepoint);
        if (iter != entry.chars.end())
        {
            return iter->second;
        }

        return rasterize(entry, codepoint);
    }

    const FontCache::Char& FontCache::rasterize(FontEntry& entry, char32_t codepoint)
    {
        // Prefer the primary face so Latin text keeps its original look; only
        // glyphs it lacks (typically CJK) come from the fallback face.
        FT_Face source = nullptr;
        if (entry.face && FT_Get_Char_Index(entry.face, codepoint) != 0)
        {
            source = entry.face;
        }
        else if (fallbackface && FT_Get_Char_Index(fallbackface, codepoint) != 0)
        {
            // The fallback face is shared between all sizes, so select the
            // entry's pixel size before rasterizing (single-threaded access).
            FT_Set_Pixel_Sizes(fallbackface, entry.pixelw, entry.pixelh);
            source = fallbackface;
        }

        if (!source || FT_Load_Char(source, codepoint, FT_LOAD_RENDER))
        {
            return store_placeholder(entry, codepoint);
        }

        FT_GlyphSlot g = source->glyph;

        Char ch{};
        ch.ax = static_cast<int16_t>(g->advance.x >> 6);
        ch.ay = static_cast<int16_t>(g->advance.y >> 6);
        ch.bw = static_cast<int16_t>(g->bitmap.width);
        ch.bh = static_cast<int16_t>(g->bitmap.rows);
        ch.bl = static_cast<int16_t>(g->bitmap_left);
        ch.bt = static_cast<int16_t>(g->bitmap_top);

        if (ch.bw > 0 && ch.bh > 0)
        {
            bool uploaded = GraphicsGL::get().upload_glyph(
                ch.bw, ch.bh, static_cast<int16_t>(g->bitmap.pitch), g->bitmap.buffer, ch.ox, ch.oy);
            if (!uploaded)
            {
                // Atlas space ran out: keep the metrics so layouts still work,
                // but the character stays invisible.
                ch.bw = 0;
                ch.bh = 0;
                if (!atlasfullwarned)
                {
                    atlasfullwarned = true;
                    Console::get().print("Warning: font atlas is full, new glyphs will be skipped.");
                }
            }
        }

        return entry.chars.emplace(codepoint, ch).first->second;
    }

    const FontCache::Char& FontCache::store_placeholder(FontEntry& entry, char32_t codepoint)
    {
        Char ch{};
        // Keep an advance so unknown characters leave a visible gap rather
        // than collapsing the surrounding text together.
        ch.ax = static_cast<int16_t>(entry.pixelh / 2);
        return entry.chars.emplace(codepoint, ch).first->second;
    }
}
