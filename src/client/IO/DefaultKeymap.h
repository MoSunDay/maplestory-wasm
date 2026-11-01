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
    /// Bare v83 layout shared by the runtime keyboard fallback and the
    /// keyconfig window, so a partial server keymap still leaves every
    /// standard action reachable (CTRL attack included).
    inline std::map<int32_t, Keyboard::Mapping> default_basic_mappings()
    {
        return {
            { KeyConfig::ESCAPE, { KeyType::MENU, KeyAction::MAINMENU } },
            { KeyConfig::F1, { KeyType::FACE, KeyAction::FACE1 } },
            { KeyConfig::F2, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 1) } },
            { KeyConfig::F3, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 2) } },
            { KeyConfig::F5, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 3) } },
            { KeyConfig::F6, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 4) } },
            { KeyConfig::F7, { KeyType::FACE, KeyAction::actionbyid(static_cast<int32_t>(KeyAction::FACE1) + 5) } },
            { KeyConfig::F8, { KeyType::FACE, KeyAction::FACE7 } },
            { KeyConfig::GRAVE_ACCENT, { KeyType::MENU, KeyAction::CASHSHOP } },
            { KeyConfig::NUM1, { KeyType::MENU, KeyAction::CHATALL } },
            { KeyConfig::NUM2, { KeyType::MENU, KeyAction::CHATPT } },
            { KeyConfig::NUM3, { KeyType::MENU, KeyAction::CHATBUDDY } },
            { KeyConfig::NUM4, { KeyType::MENU, KeyAction::CHATGUILD } },
            { KeyConfig::NUM5, { KeyType::MENU, KeyAction::CHATALLIANCE } },
            { KeyConfig::Q, { KeyType::MENU, KeyAction::QUESTLOG } },
            { KeyConfig::W, { KeyType::MENU, KeyAction::WORLDMAP } },
            { KeyConfig::E, { KeyType::MENU, KeyAction::EQUIPS } },
            { KeyConfig::R, { KeyType::MENU, KeyAction::BUDDYLIST } },
            { KeyConfig::T, { KeyType::MENU, KeyAction::BOSS } },
            { KeyConfig::Y, { KeyType::MENU, KeyAction::ITEMPOT } },
            { KeyConfig::I, { KeyType::MENU, KeyAction::INVENTORY } },
            { KeyConfig::P, { KeyType::MENU, KeyAction::PARTY } },
            { KeyConfig::LEFT_BRACKET, { KeyType::MENU, KeyAction::MAINMENU } },
            { KeyConfig::RIGHT_BRACKET, { KeyType::MENU, KeyAction::TOGGLEQS } },
            { KeyConfig::BACKSLASH, { KeyType::MENU, KeyAction::KEYCONFIG } },
            { KeyConfig::G, { KeyType::MENU, KeyAction::GUILD } },
            { KeyConfig::H, { KeyType::MENU, KeyAction::WHISPER } },
            { KeyConfig::K, { KeyType::MENU, KeyAction::SKILLBOOK } },
            { KeyConfig::L, { KeyType::MENU, KeyAction::HELPER } },
            { KeyConfig::SEMICOLON, { KeyType::MENU, KeyAction::GMSMEDALS } },
            { KeyConfig::APOSTROPHE, { KeyType::MENU, KeyAction::CHATWINDOW } },
            { KeyConfig::Z, { KeyType::ACTION, KeyAction::PICKUP } },
            { KeyConfig::X, { KeyType::ACTION, KeyAction::SIT } },
            { KeyConfig::C, { KeyType::MENU, KeyAction::CHARSTATS } },
            { KeyConfig::V, { KeyType::MENU, KeyAction::EVENT } },
            { KeyConfig::B, { KeyType::MENU, KeyAction::PROFESSION } },
            { KeyConfig::M, { KeyType::MENU, KeyAction::MINIMAP } },
            { KeyConfig::LEFT_CONTROL, { KeyType::ACTION, KeyAction::ATTACK } },
            { KeyConfig::RIGHT_CONTROL, { KeyType::ACTION, KeyAction::ATTACK } },
            { KeyConfig::LEFT_ALT, { KeyType::ACTION, KeyAction::JUMP } },
            { KeyConfig::RIGHT_ALT, { KeyType::ACTION, KeyAction::JUMP } },
            { KeyConfig::SPACE, { KeyType::ACTION, KeyAction::NPCCHAT } }
        };
    }
}
