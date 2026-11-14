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
#include "Spawn.h"

#include "MapleMap/Npc.h"
#include "MapleMap/Mob.h"
#include "MapleMap/Reactor.h"
#include "MapleMap/ItemDrop.h"
#include "MapleMap/MesoDrop.h"
#include "../Character/OtherChar.h"

namespace jrc
{
    namespace
    {
        // The server spawns a character entering the field 42px above its
        // foothold with a falling stance. Remote characters never simulate
        // local physics, so without a correction they would hang in the air.
        constexpr int16_t MIN_AIRBORNE_SNAP = 1;
        constexpr int16_t MAX_AIRBORNE_SNAP = 200;

        // Stance bytes pair up per state (mirrored adds one), and FALL is
        // the only airborne state in the movement encoding.
        bool is_airborne_stance(int8_t stancebyte)
        {
            return (stancebyte & ~static_cast<int8_t>(1)) == Char::FALL;
        }

        Point<int16_t> snap_airborne_spawn(
            const Physics& physics,
            Point<int16_t> position)
        {
            Point<int16_t> ground = physics.get_y_below(position);
            int16_t drop = ground.y() - position.y();
            if (drop < MIN_AIRBORNE_SNAP || drop > MAX_AIRBORNE_SNAP)
                return position;
            return Point<int16_t>(position.x(), ground.y());
        }
    }

    NpcSpawn::NpcSpawn(int32_t o, int32_t i, Point<int16_t> p, bool fl, uint16_t f)
        : oid(o), id(i), position(p), flip(fl), fh(f) {}

    int32_t NpcSpawn::get_oid() const
    {
        return oid;
    }

    std::unique_ptr<MapObject> NpcSpawn::instantiate(const Physics& physics) const
    {
        auto spawnposition = physics.get_y_below(position);
        return std::make_unique<Npc>(id, oid, flip, fh, false, spawnposition);
    }


    MobSpawn::MobSpawn(int32_t o, int32_t i, int8_t m, int8_t st, uint16_t f, bool ns, int8_t t, Point<int16_t> p)
        : oid(o), id(i), mode(m), stance(st), fh(f), newspawn(ns), team(t), position(p) {}

    int8_t MobSpawn::get_mode() const
    {
        return mode;
    }

    int32_t MobSpawn::get_oid() const
    {
        return oid;
    }

    int8_t MobSpawn::get_stance() const
    {
        return stance;
    }

    uint16_t MobSpawn::get_foothold() const
    {
        return fh;
    }

    Point<int16_t> MobSpawn::get_position() const
    {
        return position;
    }

    std::unique_ptr<MapObject> MobSpawn::instantiate() const
    {
        return std::make_unique<Mob>(oid, id, mode, stance, fh, newspawn, team, position);
    }


    ReactorSpawn::ReactorSpawn(int32_t o, int32_t r, int8_t s, Point<int16_t> p)
        : oid(o), rid(r), state(s), position(p) {}

    int32_t ReactorSpawn::get_oid() const
    {
        return oid;
    }

    std::unique_ptr<MapObject> ReactorSpawn::instantiate(const Physics& physics) const
    {
        auto spawnposition = physics.get_y_below(position);
        return std::make_unique<Reactor>(oid, rid, state, spawnposition);
    }


    DropSpawn::DropSpawn(int32_t o, int32_t i, bool ms, int32_t ow, Point<int16_t> p, Point<int16_t> d, int8_t t, int8_t m, bool pd)
        : oid(o), id(i), meso(ms), owner(ow), start(p), dest(d), droptype(t), mode(m), playerdrop(pd) {}

    bool DropSpawn::is_meso() const
    {
        return meso;
    }

    int32_t DropSpawn::get_itemid() const
    {
        return id;
    }

    int32_t DropSpawn::get_oid() const
    {
        return oid;
    }

    int32_t DropSpawn::get_owner() const
    {
        return owner;
    }

    int8_t DropSpawn::get_drop_type() const
    {
        return droptype;
    }

    bool DropSpawn::is_player_drop() const
    {
        return playerdrop;
    }

    std::unique_ptr<MapObject> DropSpawn::instantiate(const Animation& icon) const
    {
        return std::make_unique<MesoDrop>(oid, owner, start, dest, droptype, mode, playerdrop, icon);
    }

    std::unique_ptr<MapObject> DropSpawn::instantiate(const Texture& icon) const
    {
        return std::make_unique<ItemDrop>(oid, owner, start, dest, droptype, mode, id, playerdrop, icon);
    }


    CharSpawn::CharSpawn(int32_t c,
                         const LookEntry& lk,
                         uint16_t l,
                         int16_t j,
                         const std::string& nm,
                         int8_t st,
                         Point<int16_t> p,
                         int16_t combo,
                         int32_t chair,
                         const std::string& board)
        : cid(c), level(l), job(j), name(nm), stance(st), position(p),
          combo_value(combo), chair_id(chair), chalkboard(board), look(lk) {}

    int32_t CharSpawn::get_cid() const
    {
        return cid;
    }

    Point<int16_t> CharSpawn::get_position() const
    {
        return position;
    }

    int8_t CharSpawn::get_stance() const
    {
        return stance;
    }

    Point<int16_t> CharSpawn::resolve_position(const Physics& physics) const
    {
        if (!is_airborne_stance(stance))
            return position;
        return snap_airborne_spawn(physics, position);
    }

    int32_t CharSpawn::get_chair_id() const
    {
        return chair_id;
    }

    const std::string& CharSpawn::get_chalkboard() const
    {
        return chalkboard;
    }

    void CharSpawn::set_pets(const PetSpawnEntry* entries, uint8_t count)
    {
        petcount = count < 3 ? count : 3;
        for (uint8_t i = 0; i < petcount; ++i)
        {
            pets[i] = entries[i];
        }
    }

    const PetSpawnEntry* CharSpawn::get_pets() const
    {
        return pets;
    }

    uint8_t CharSpawn::get_pet_count() const
    {
        return petcount;
    }

    std::unique_ptr<MapObject> CharSpawn::instantiate(const Physics& physics) const
    {
        auto character = std::make_unique<OtherChar>(
            cid, look, level, job, name, stance, resolve_position(physics));
        if (combo_value > 0)
            character->set_visual_buff(Buffstat::COMBO, combo_value);
        return character;
    }
}
