#pragma once

#include "../../../Util/Utf8.h"

#include <array>
#include <string>
#include <string_view>

namespace jrc::CharacterCreation
{
    inline bool is_ascii_alphanumeric(char32_t codepoint)
    {
        return (codepoint >= U'0' && codepoint <= U'9') ||
            (codepoint >= U'A' && codepoint <= U'Z') ||
            (codepoint >= U'a' && codepoint <= U'z');
    }

    inline bool is_obviously_non_alphanumeric(char32_t codepoint)
    {
        if (codepoint <= 0x7F)
        {
            return !is_ascii_alphanumeric(codepoint);
        }

        if ((codepoint >= 0x80 && codepoint <= 0x9F) ||
            (codepoint >= 0x2000 && codepoint <= 0x206F) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            (codepoint >= 0xE000 && codepoint <= 0xF8FF))
        {
            return true;
        }

        switch (codepoint)
        {
        case 0x00A0: // No-break space
        case 0x1680:
        case 0x3000: // Ideographic space
        case 0x3001: // Ideographic comma
        case 0x3002: // Ideographic full stop
        case 0x3008:
        case 0x3009:
        case 0x300A:
        case 0x300B:
        case 0x300C:
        case 0x300D:
        case 0x300E:
        case 0x300F:
        case 0x3010:
        case 0x3011:
        case 0x3014:
        case 0x3015:
        case 0x3016:
        case 0x3017:
        case 0x3018:
        case 0x3019:
        case 0x301A:
        case 0x301B:
        case 0xFF01:
        case 0xFF08:
        case 0xFF09:
        case 0xFF0C:
        case 0xFF0E:
        case 0xFF1A:
        case 0xFF1B:
        case 0xFF1F:
            return true;
        default:
            return false;
        }
    }

    inline std::string ascii_lowercase(std::string value)
    {
        for (char& byte : value)
        {
            if (byte >= 'A' && byte <= 'Z')
            {
                byte = static_cast<char>(byte + ('a' - 'A'));
            }
        }
        return value;
    }

    inline bool contains_reserved_fragment(const std::string& name)
    {
        static constexpr std::array<std::string_view, 58> RESERVED = {
            "admin", "owner", "moderator", "intern", "donor", "administrator", "fredrick",
            "help", "helper", "alert", "notice", "maplestory", "fuck", "wizet", "fucking",
            "negro", "fuk", "fuc", "penis", "pussy", "asshole", "gay", "nigger", "homo",
            "suck", "cum", "shit", "shitty", "condom", "security", "official", "rape",
            "nigga", "sex", "tit", "boner", "orgy", "clit", "fatass", "bitch", "support",
            "gamemaster", "cock", "gaay", "gm", "operate", "master", "sysop", "party",
            "community", "message", "event", "test", "meso", "scania", "yata", "asiasoft",
            "henesys"
        };

        const std::string lower_name = ascii_lowercase(name);
        for (std::string_view fragment : RESERVED)
        {
            if (lower_name.find(fragment) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    // This mirrors the stable, user-visible part of Cosmic's name policy.
    // The server remains authoritative for the full Unicode category check
    // and uniqueness, so uncommon non-ASCII scripts are not rejected here.
    inline bool is_locally_valid_name(const std::string& name)
    {
        if (name.size() < 3 || name.size() > 12)
        {
            return false;
        }

        for (size_t offset = 0; offset < name.size();)
        {
            size_t length = Utf8::sequence_length(name[offset]);
            char32_t codepoint = Utf8::decode(name.data() + offset, name.size() - offset);
            if (codepoint == Utf8::REPLACEMENT || codepoint > 0xFFFF ||
                offset + length > name.size() || is_obviously_non_alphanumeric(codepoint))
            {
                return false;
            }
            offset += length;
        }

        return !contains_reserved_fragment(name);
    }
}
