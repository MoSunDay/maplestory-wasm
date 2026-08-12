#include "MapChars.h"

namespace jrc
{
    void MapChars::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
    {
        chars.draw(layer, viewx, viewy, alpha);
    }

    void MapChars::update(const Physics& physics)
    {
        for (; !spawns.empty(); spawns.pop())
        {
            const CharSpawn& spawn = spawns.front();

            int32_t cid = spawn.get_cid();
            Optional<OtherChar> ochar = get_char(cid);
            if (ochar)
            {

            }
            else
            {
                chars.add(
                    spawn.instantiate()
                );
            }
        }

        chars.update(physics);
    }

    void MapChars::spawn(CharSpawn&& spawn)
    {
        spawns.emplace(
            std::move(spawn)
        );
    }

    void MapChars::remove(int32_t cid)
    {
        chars.remove(cid);
    }

    void MapChars::clear()
    {
        chars.clear();
    }

    void MapChars::send_movement(int32_t cid, const std::vector<Movement>& movements)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->send_movement(movements);
        }
    }

    void MapChars::update_look(int32_t cid, const LookEntry& look)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->update_look(look);
        }
    }

    void MapChars::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
    {
        if (Optional<OtherChar> otherchar = get_char(cid))
        {
            otherchar->set_party_hp(hp, max_hp);
        }
    }

    void MapChars::clear_party_member_hp()
    {
        for (auto& iter : chars)
        {
            Optional<OtherChar> otherchar = iter.second.get();
            if (otherchar)
            {
                otherchar->clear_party_hp();
            }
        }
    }

    void MapChars::set_visual_buff(int32_t cid, Buffstat::Id stat, int16_t value)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->set_visual_buff(stat, value);
    }

    void MapChars::set_visual_buff_source(int32_t cid, Buffstat::Id stat, int32_t source)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->set_visual_buff_source(stat, source);
    }

    void MapChars::cancel_visual_buff(int32_t cid, Buffstat::Id stat)
    {
        if (Optional<OtherChar> character = get_char(cid))
            character->cancel_visual_buff(stat);
    }

    Optional<OtherChar> MapChars::get_char(int32_t cid)
    {
        return chars.get(cid);
    }
}
