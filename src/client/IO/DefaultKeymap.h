//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright (C) 2015-2019 Daniel Allendorf, Ryan Payton                    //
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Keyboard.h"
#include "KeyConfig.h"

#include <map>

namespace jrc
{
    /// Official v83 default keyset, mirroring entry-by-entry the table the
    /// game server installs on character creation (GameConstants
    /// DEFAULT_KEY/DEFAULT_TYPE/DEFAULT_ACTION in link_repos/MapleStory-Server).
    /// The runtime fallback and the keyconfig "default" reset therefore
    /// always agree with a fresh official character. Server keymap packets
    /// overwrite these bindings entry-by-entry; NONE entries never clear a
    /// preset binding, so a partial keymap still leaves every standard
    /// action reachable (CTRL attack included).
    inline std::map<int32_t, Keyboard::Mapping> default_basic_mappings()
    {
        return {
            // Function row: F1 through F7 play the seven expressions.
            // (Post-Chaos layouts skip F4 and shift the faces right; v83
            // does not.)
            { KeyConfig::F1, { KeyType::FACE, KeyAction::FACE1 } },
            { KeyConfig::F2, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 1) } },
            { KeyConfig::F3, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 2) } },
            { KeyConfig::F4, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 3) } },
            { KeyConfig::F5, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 4) } },
            { KeyConfig::F6, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 5) } },
            { KeyConfig::F7, { KeyType::FACE, KeyAction::FACE7 } },

            // Number row: chat target selectors; grave opens the cash shop.
            { KeyConfig::NUM1, { KeyType::MENU, KeyAction::CHATALL } },
            { KeyConfig::NUM2, { KeyType::MENU, KeyAction::CHATPT } },
            { KeyConfig::NUM3, { KeyType::MENU, KeyAction::CHATBUDDY } },
            { KeyConfig::NUM4, { KeyType::MENU, KeyAction::CHATGUILD } },
            { KeyConfig::NUM5, { KeyType::MENU, KeyAction::CHATALLIANCE } },
            { KeyConfig::NUM6, { KeyType::MENU, KeyAction::CHATSPOUSE } },
            { KeyConfig::GRAVE_ACCENT, { KeyType::MENU, KeyAction::CASHSHOP } },

            // QWERTY row.
            { KeyConfig::Q, { KeyType::MENU, KeyAction::QUESTLOG } },
            { KeyConfig::W, { KeyType::MENU, KeyAction::WORLDMAP } },
            { KeyConfig::E, { KeyType::MENU, KeyAction::EQUIPS } },
            { KeyConfig::R, { KeyType::MENU, KeyAction::BUDDYLIST } },
            { KeyConfig::I, { KeyType::MENU, KeyAction::INVENTORY } },
            { KeyConfig::O, { KeyType::MENU, KeyAction::PARTYSEARCH } },
            { KeyConfig::P, { KeyType::MENU, KeyAction::PARTY } },
            { KeyConfig::LEFT_BRACKET, { KeyType::MENU, KeyAction::MAINMENU } },
            { KeyConfig::RIGHT_BRACKET, { KeyType::MENU, KeyAction::TOGGLEQS } },
            { KeyConfig::BACKSLASH, { KeyType::MENU, KeyAction::KEYCONFIG } },

            // Home row.
            { KeyConfig::S, { KeyType::MENU, KeyAction::CHARSTATS } },
            { KeyConfig::F, { KeyType::MENU, KeyAction::FAMILY } },
            { KeyConfig::G, { KeyType::MENU, KeyAction::GUILD } },
            { KeyConfig::H, { KeyType::MENU, KeyAction::WHISPER } },
            { KeyConfig::K, { KeyType::MENU, KeyAction::SKILLBOOK } },
            { KeyConfig::L, { KeyType::MENU, KeyAction::HELPER } },
            { KeyConfig::SEMICOLON, { KeyType::MENU, KeyAction::GMSMEDALS } },
            { KeyConfig::APOSTROPHE, { KeyType::MENU, KeyAction::CHATWINDOW } },

            // Bottom row.
            { KeyConfig::Z, { KeyType::ACTION, KeyAction::PICKUP } },
            { KeyConfig::X, { KeyType::ACTION, KeyAction::SIT } },
            { KeyConfig::C, { KeyType::MENU, KeyAction::MESSAGE } },
            { KeyConfig::B, { KeyType::MENU, KeyAction::MONSTERBOOK } },
            { KeyConfig::M, { KeyType::MENU, KeyAction::MINIMAP } },

            // Modifiers. LEFT_* mirror the official keyset; RIGHT_* are a
            // client-side convenience for right-handed players and are not
            // part of the server's default rows.
            { KeyConfig::LEFT_CONTROL, { KeyType::ACTION, KeyAction::ATTACK } },
            { KeyConfig::LEFT_ALT, { KeyType::ACTION, KeyAction::JUMP } },
            { KeyConfig::RIGHT_CONTROL, { KeyType::ACTION, KeyAction::ATTACK } },
            { KeyConfig::RIGHT_ALT, { KeyType::ACTION, KeyAction::JUMP } },
            { KeyConfig::SPACE, { KeyType::ACTION, KeyAction::NPCCHAT } }
        };
    }
}
