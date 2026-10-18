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
#pragma once
#include "MapObject.h"

#include "../../Template/Interpolated.h"
#include "../../Template/Rectangle.h"
#include "DropLifecycle/RemovalPolicy.h"

#include <optional>

namespace jrc
{
    class Drop : public MapObject
    {
    public:
        virtual int8_t update(const Physics& physics) override;

        void init(int8_t);
        void expire(drop_removal::Effect, const PhysicsObject*,
            std::optional<Point<int16_t>> pickup_target = std::nullopt);

        Rectangle<int16_t> bounds() const;
        int32_t get_owner() const;
        int8_t get_pickup_type() const;
        bool is_player_drop() const;
        bool is_lootable() const;
        void update_ownership(int32_t owner, int8_t pickup_type, bool player_drop);
        virtual bool is_meso() const = 0;
        virtual int32_t get_item_id() const = 0;

    protected:
        Drop(int32_t oid, int32_t owner, Point<int16_t> start,
            Point<int16_t> dest, int8_t type, int8_t mode, bool playerdrop);

        Linear<float> opacity;
        Linear<float> angle;

    private:
        enum State
        {
            DROPPED,
            FLOATING,
            PICKEDUP
        };

        int32_t owner;
        int8_t pickuptype;
        bool playerdrop;

        const PhysicsObject* looter;
        std::optional<Point<int16_t>> pickup_target;
        State state;

        Point<int16_t> dest;
        double basey;
        double moved;
    };
}
