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
#include "Job.h"
#include "JobProgression.h"

namespace jrc
{
    Job::Job(uint16_t i)
    {
        change_job(i);
    }

    Job::Job()
    {
        change_job(0);
    }

    void Job::change_job(uint16_t i)
    {
        id   = i;
        name = get_name(id);

        level = static_cast<Level>(job_progression::stage_for(id));
    }

    bool Job::is_sub_job(uint16_t subid) const
    {
        for (int32_t lvit = BEGINNER; lvit <= FOURTH; ++lvit)
        {
            Level lv = static_cast<Level>(lvit);
            uint16_t stage_job = get_subjob(lv);
            bool explorer_beginner = lv == BEGINNER && id < 1000;
            if (subid == stage_job && (stage_job != 0 || explorer_beginner))
            {
                return true;
            }
        }

        return false;
    }

    bool Job::can_use(int32_t skill_id) const
    {
        auto required = static_cast<uint16_t>(skill_id / 10000);
        return is_sub_job(required);
    }

    uint16_t Job::get_id() const
    {
        return id;
    }

    uint16_t Job::get_subjob(Level lv) const
    {
        return job_progression::job_for_stage(
            id,
            static_cast<job_progression::Stage>(lv)
        );
    }

    const std::string& Job::get_name() const
    {
        return name;
    }

    Job::Level Job::get_level() const
    {
        return level;
    }

    std::string Job::get_name(uint16_t jid) const
    {
        switch (jid)
        {
        case 0:
            return "新手";
        case 100:
            return "战士";
        case 110:
            return "剑客";
        case 111:
            return "勇士";
        case 112:
            return "英雄";
        case 120:
            return "准骑士";
        case 121:
            return "骑士";
        case 122:
            return "圣骑士";
        case 130:
            return "枪战士";
        case 131:
            return "龙骑士";
        case 132:
            return "黑骑士";
        case 200:
            return "魔法师";
        case 210:
            return "火毒法师";
        case 211:
            return "火毒巫师";
        case 212:
            return "火毒魔导师";
        case 220:
            return "冰雷法师";
        case 221:
            return "冰雷巫师";
        case 222:
            return "冰雷魔导师";
        case 230:
            return "牧师";
        case 231:
            return "祭司";
        case 232:
            return "主教";
        case 300:
            return "弓箭手";
        case 310:
            return "猎人";
        case 311:
            return "游侠";
        case 312:
            return "神射手";
        case 320:
            return "弩弓手";
        case 321:
            return "狙击手";
        case 322:
            return "神弩手";
        case 400:
            return "飞侠";
        case 410:
            return "刺客";
        case 411:
            return "无影人";
        case 412:
            return "隐士";
        case 420:
            return "侠盗";
        case 421:
            return "神偷";
        case 422:
            return "影武者";
        case 500:
            return "海盗";
        case 510:
            return "拳手";
        case 511:
            return "斗士";
        case 512:
            return "拳霸";
        case 520:
            return "枪手";
        case 521:
            return "大副";
        case 522:
            return "枪神";
        case 1000:
            return "贵族";
        case 1100:
            return "魂骑士";
        case 1110:
            return "魂骑士(2转)";
        case 1111:
            return "魂骑士(3转)";
        case 1112:
            return "魂骑士(4转)";
        case 1200:
            return "炎术士";
        case 1210:
            return "炎术士(2转)";
        case 1211:
            return "炎术士(3转)";
        case 1212:
            return "炎术士(4转)";
        case 1300:
            return "风灵使者";
        case 1310:
            return "风灵使者(2转)";
        case 1311:
            return "风灵使者(3转)";
        case 1312:
            return "风灵使者(4转)";
        case 1400:
            return "夜行者";
        case 1410:
            return "夜行者(2转)";
        case 1411:
            return "夜行者(3转)";
        case 1412:
            return "夜行者(4转)";
        case 1500:
            return "奇袭者";
        case 1510:
            return "奇袭者(2转)";
        case 1511:
            return "奇袭者(3转)";
        case 1512:
            return "奇袭者(4转)";
        case 2000:
        case 2100:
        case 2110:
        case 2111:
        case 2112:
            return "战神";
        default:
            return "";
        }
    }

    Equipstat::Id Job::get_primary(Weapon::Type weapontype) const
    {
        switch (job_progression::archetype_for(id))
        {
        case 2:
            return Equipstat::INT;
        case 3:
            return Equipstat::DEX;
        case 4:
            return Equipstat::LUK;
        case 5:
            return (weapontype == Weapon::GUN) ? Equipstat::DEX : Equipstat::STR;
        default:
            return Equipstat::STR;
        }
    }

    Equipstat::Id Job::get_secondary(Weapon::Type weapontype) const
    {
        switch (job_progression::archetype_for(id))
        {
        case 2:
            return Equipstat::LUK;
        case 3:
            return Equipstat::STR;
        case 4:
            return Equipstat::DEX;
        case 5:
            return (weapontype == Weapon::GUN) ? Equipstat::STR : Equipstat::DEX;
        default:
            return Equipstat::DEX;
        }
    }
}
