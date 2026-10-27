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
#include "DrawArgument.h"

#include "nlnx/node.hpp"
#include "nlnx/bitmap.hpp"

namespace jrc
{
    // Represents a single image loaded from a of game data.
    class Texture
    {
    public:
        enum class LoadMode
        {
            BACKGROUND,
            DEFERRED
        };

        Texture(nl::node source, LoadMode load_mode = LoadMode::BACKGROUND);
        Texture();
        ~Texture();

        void draw(const DrawArgument& args) const;
        /// Draw only a sub-rectangle of the bitmap, given in bitmap
        /// pixels, into the destination this texture would otherwise
        /// cover, so callers can skip baked-in art that must not show.
        void draw(
            const DrawArgument& args,
            const Rectangle<int16_t>& source
        ) const;
        /// Draw the texture and return the exact destination rectangle
        /// (pre-clip) used by that same draw call.
        Rectangle<int16_t> draw_bounds(const DrawArgument& args) const;
        bool draw_effect(const DrawArgument& args) const;
        void prepare_visible() const;
        void prepare_background() const;
        void prepare_map_required() const;
        void prepare_effect() const;
        void shift(Point<int16_t> amount);

        bool is_valid() const;
        bool is_ready() const;
        int16_t width() const;
        int16_t height() const;
        Point<int16_t> get_origin() const;
        Point<int16_t> get_dimensions() const;

    private:
        enum class DrawLoadClass
        {
            BLOCKING_VISIBLE,
            TRANSIENT_EFFECT
        };

        bool draw_with_load_class(const DrawArgument& args, DrawLoadClass load_class,
            Rectangle<int16_t>* bounds = nullptr,
            const Rectangle<int16_t>* source = nullptr) const;

        nl::bitmap bitmap;
        Point<int16_t> origin;
        Point<int16_t> dimensions;
    };
}
