#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace jrc
{
    // Minimal UTF-8 helpers: the client treats all user-visible text as UTF-8,
    // matching the network protocol, so every layer that touches text bytes
    // needs codepoint-aware iteration instead of raw byte arithmetic.
    namespace Utf8
    {
        constexpr char32_t REPLACEMENT = 0xFFFD;

        inline bool is_continuation(char byte)
        {
            return (static_cast<uint8_t>(byte) & 0xC0) == 0x80;
        }

        // Length in bytes of the sequence starting with the given lead byte.
        // Invalid lead bytes are treated as single-byte sequences so callers
        // always make forward progress.
        inline size_t sequence_length(char lead)
        {
            uint8_t byte = static_cast<uint8_t>(lead);
            if (byte < 0x80)
            {
                return 1;
            }
            if ((byte & 0xE0) == 0xC0)
            {
                return 2;
            }
            if ((byte & 0xF0) == 0xE0)
            {
                return 3;
            }
            if ((byte & 0xF8) == 0xF0)
            {
                return 4;
            }
            return 1;
        }

        // Decode one codepoint at the start of data (up to size bytes long).
        // Returns the replacement character for malformed sequences.
        inline char32_t decode(const char* data, size_t size)
        {
            if (size == 0)
            {
                return 0;
            }

            uint8_t lead = static_cast<uint8_t>(data[0]);
            if (lead < 0x80)
            {
                return lead;
            }

            size_t length = sequence_length(data[0]);
            if (length == 1 || length > size)
            {
                return REPLACEMENT;
            }

            char32_t codepoint;
            switch (length)
            {
            case 2:
                codepoint = lead & 0x1F;
                break;
            case 3:
                codepoint = lead & 0x0F;
                break;
            default:
                codepoint = lead & 0x07;
                break;
            }

            for (size_t i = 1; i < length; ++i)
            {
                if (!is_continuation(data[i]))
                {
                    return REPLACEMENT;
                }
                codepoint = (codepoint << 6) | (static_cast<uint8_t>(data[i]) & 0x3F);
            }

            // Reject overlong encodings, surrogates and out-of-range values.
            if (codepoint < 0x80 ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
                codepoint > 0x10FFFF ||
                (length == 2 && codepoint < 0x80) ||
                (length == 3 && codepoint < 0x800) ||
                (length == 4 && codepoint < 0x10000))
            {
                return REPLACEMENT;
            }

            return codepoint;
        }

        // Convert a UTF-16 offset (as used by browser text controls) to the
        // byte offset of the codepoint it points at. Offsets past the end are
        // clamped; surrogate halves snap to the leading codepoint boundary.
        inline size_t utf16_to_byte_offset(const std::string& text, size_t utf16pos)
        {
            size_t utf16 = 0;
            size_t bytepos = 0;
            while (bytepos < text.size() && utf16 < utf16pos)
            {
                char32_t codepoint = decode(text.data() + bytepos, text.size() - bytepos);
                utf16 += (codepoint >= 0x10000) ? 2 : 1;
                bytepos += sequence_length(text[bytepos]);
            }
            return bytepos;
        }

        // Convert a byte offset (snapped to a codepoint boundary by callers)
        // to the corresponding UTF-16 offset.
        inline size_t byte_to_utf16_offset(const std::string& text, size_t bytepos)
        {
            size_t utf16 = 0;
            size_t position = 0;
            while (position < text.size() && position < bytepos)
            {
                char32_t codepoint = decode(text.data() + position, text.size() - position);
                utf16 += (codepoint >= 0x10000) ? 2 : 1;
                position += sequence_length(text[position]);
            }
            return utf16;
        }

        // Codepoints after which (or before which) a line may be broken.
        // CJK scripts do not use spaces, so every ideograph, kana or syllable
        // is its own wrapping opportunity.
        inline bool is_cjk(char32_t codepoint)
        {
            return
                (codepoint >= 0x1100 && codepoint <= 0x11FF) ||   // Hangul Jamo
                (codepoint >= 0x2E80 && codepoint <= 0x30FF) ||   // Radicals, Kangxi, CJK punctuation, Kana
                (codepoint >= 0x3130 && codepoint <= 0x318F) ||   // Hangul Compatibility Jamo
                (codepoint >= 0x31A0 && codepoint <= 0x31BF) ||   // Bopomofo Extended
                (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||   // CJK Extension A
                (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||   // CJK Unified Ideographs
                (codepoint >= 0xA000 && codepoint <= 0xA4CF) ||   // Yi
                (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||   // Hangul Syllables
                (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||   // CJK Compatibility Ideographs
                (codepoint >= 0xFE30 && codepoint <= 0xFE4F) ||   // CJK Compatibility Forms
                (codepoint >= 0xFF00 && codepoint <= 0xFFEF) ||   // Halfwidth and Fullwidth Forms
                (codepoint >= 0x20000 && codepoint <= 0x3FFFF);   // CJK Extensions B..F
        }
    }
}
