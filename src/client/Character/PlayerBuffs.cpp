#include "Player.h"

#include "../Constants.h"

namespace jrc
{
    void Player::update_buffs()
    {
        bool expired = false;
        for (Buff& buff : buffs.values())
        {
            if (buff.remaining_ms <= 0)
                continue;

            buff.remaining_ms -= Constants::TIMESTEP;
            if (buff.remaining_ms > 0)
                continue;

            buff = {};
            expired = true;
        }

        if (expired)
            recalc_stats(false);
    }

    void Player::give_buff(Buff buff)
    {
        buff.remaining_ms = buff_remaining_from_duration(buff.duration);
        buffs[buff.stat] = buff;
    }

    void Player::cancel_buff(Buffstat::Id stat)
    {
        buffs[stat] = {};
    }

    bool Player::has_buff(Buffstat::Id stat) const
    {
        return buff_is_active(buffs[stat]);
    }

    bool Player::has_skill_buff(int32_t skill_id) const
    {
        if (skill_id <= 0)
            return false;

        for (const Buff& buff : buffs.values())
        {
            if (buff_is_active(buff) && buff.skillid == skill_id)
                return true;
        }
        return false;
    }

    int16_t Player::get_visual_buff_value(Buffstat::Id stat) const
    {
        return buffs[stat].value;
    }

    int32_t Player::get_visual_buff_source(Buffstat::Id stat) const
    {
        return buffs[stat].skillid;
    }
}
