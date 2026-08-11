#pragma once

#include "Text.h"

#include "../Template/Singleton.h"

#include "ft2build.h"
#include FT_FREETYPE_H

#include <cstdint>
#include <unordered_map>

namespace jrc
{
    // Glyph storage with per-codepoint lazy rasterization.
    //
    // The original renderer pre-baked ASCII 32-127 into fixed per-byte tables,
    // which cannot represent CJK or any non-Latin text. FontCache keeps one
    // FreeType face per configured font size (the primary face, e.g. Arial,
    // to preserve the original look) plus a shared fallback face covering CJK.
    // Glyphs are rasterized into the shared atlas on first use, so text in any
    // script renders correctly without baking entire character sets up front.
    class FontCache : public Singleton<FontCache>
    {
    public:
        struct Char
        {
            int16_t ax; // horizontal advance
            int16_t ay; // vertical advance
            int16_t bw; // bitmap width
            int16_t bh; // bitmap height
            int16_t bl; // bitmap left bearing
            int16_t bt; // bitmap top bearing
            int16_t ox; // atlas region x
            int16_t oy; // atlas region y
        };

        // Store the FreeType library handle and load the fallback face.
        void init(FT_Library library);

        // Load the primary face for a font id and pre-bake printable ASCII,
        // keeping startup behaviour identical to the fixed-table renderer.
        bool addfont(const char* name, Text::Font id, FT_UInt pixelw, FT_UInt pixelh);

        // Line height for the given font id (matches the legacy metric).
        int16_t linespace(Text::Font id) const;

        // Glyph metrics for a codepoint, rasterized on first use. Never fails:
        // characters no face provides get an invisible placeholder with a
        // sensible advance so layouts stay stable.
        const Char& getglyph(Text::Font id, char32_t codepoint);

    private:
        struct FontEntry
        {
            FT_Face face = nullptr;
            FT_UInt pixelw = 0;
            FT_UInt pixelh = 0;
            int16_t height = 0;
            std::unordered_map<char32_t, Char> chars;
        };

        const Char& rasterize(FontEntry& entry, char32_t codepoint);
        const Char& store_placeholder(FontEntry& entry, char32_t codepoint);

        FT_Library ftlibrary = nullptr;
        FT_Face fallbackface = nullptr;
        bool atlasfullwarned = false;
        FontEntry fonts[Text::NUM_FONTS];
    };
}
