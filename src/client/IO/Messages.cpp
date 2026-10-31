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
#include "Messages.h"

#include "UI.h"
#include "UITypes/UIStatusBar.h"

namespace jrc
{
    const EnumMap<Messages::Type, const char*> Messages::messages =
    {
        "",
        "该武器无法使用此技能。",
        "HP 不足，无法使用该技能。",
        "MP 不足，无法使用该技能。",
        "箭矢不足，无法使用该攻击。",
        "子弹不足，无法使用该攻击。",
        "飞镖不足，无法使用该攻击。",
        "该技能尚在冷却中，无法使用。",
        "需要至少一个连击点才能使用该技能。",
        "卷轴发出光芒，神秘的力量已转移到道具上。",
        "卷轴发出光芒，但道具毫无变化。",
        "卷轴的强大力量摧毁了该道具。"
    };


    InChatMessage::InChatMessage(Messages::Type t)
    {
        type = t;
    }

    void InChatMessage::drop() const
    {
        if (type == Messages::NONE)
            return;

        if (auto statusbar = UI::get().get_element<UIStatusbar>())
            statusbar->display_message(type, UIChatbar::RED);
    }


    ForbidSkillMessage::ForbidSkillMessage(SpecialMove::ForbidReason reason, Weapon::Type weapon)
        : InChatMessage(message_by_reason(reason, weapon)) {}

    Messages::Type ForbidSkillMessage::message_by_reason(SpecialMove::ForbidReason reason, Weapon::Type weapon)
    {
        switch (reason)
        {
        case SpecialMove::FBR_WEAPONTYPE:
            return Messages::SKILL_WEAPONTYPE;
        case SpecialMove::FBR_HPCOST:
            return Messages::SKILL_HPCOST;
        case SpecialMove::FBR_MPCOST:
            return Messages::SKILL_MPCOST;
        case SpecialMove::FBR_COOLDOWN:
            return Messages::SKILL_COOLDOWN;
        case SpecialMove::FBR_NOCOMBO:
            return Messages::SKILL_NOCOMBO;
        case SpecialMove::FBR_BULLETCOST:
            return message_by_weapon(weapon);
        default:
            return Messages::NONE;
        }
    }

    Messages::Type ForbidSkillMessage::message_by_weapon(Weapon::Type weapon)
    {
        switch (weapon)
        {
        case Weapon::BOW:
        case Weapon::CROSSBOW:
            return Messages::SKILL_NOARROWS;
        case Weapon::CLAW:
            return Messages::SKILL_NOSTARS;
        default:
            return Messages::SKILL_NOBULLETS;
        }
    }
}
