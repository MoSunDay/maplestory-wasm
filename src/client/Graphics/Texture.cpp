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
#include "Texture.h"
#include "GraphicsGL.h"

#include "../Configuration.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    Texture::Texture(nl::node src)
    {
        if (src.data_type() == nl::node::type::bitmap)
        {
            std::string link = src["source"];
            if (!link.empty())
            {
                nl::node srcfile = src;
                while (srcfile != srcfile.root())
                {
                    srcfile = srcfile.root();
                }
                src = srcfile.resolve(link.substr(link.find('/') + 1));
            }

            bitmap = src;
            origin = src["origin"];
            dimensions = Point<int16_t>(bitmap.width(),  bitmap.height());

#ifdef MS_PLATFORM_WASM
            GraphicsGL::get().queuebitmap(bitmap);
#else
            GraphicsGL::get().addbitmap(bitmap);
#endif
        }
    }

    Texture::Texture() {}

    Texture::~Texture() {}

    void Texture::draw(const DrawArgument& args) const
    {
        draw_with_load_class(args, DrawLoadClass::BLOCKING_VISIBLE);
    }

    bool Texture::draw_effect(const DrawArgument& args) const
    {
        return draw_with_load_class(args, DrawLoadClass::TRANSIENT_EFFECT);
    }

    bool Texture::draw_with_load_class(
        const DrawArgument& args,
        DrawLoadClass load_class
    ) const
    {
        size_t id = bitmap.id();
        if (id == 0 || args.get_color().a() <= 0.0f)
            return false;

        const Rectangle<int16_t> destination = args.get_rectangle(origin, dimensions);
        if (!GraphicsGL::get().isonscreen(destination))
        {
            return false;
        }

        if (!GraphicsGL::get().hasbitmap(bitmap))
        {
#ifdef MS_PLATFORM_WASM
            // Construction already scheduled this bitmap. Re-queueing here
            // also recovers after an atlas clear without blocking the frame.
            GraphicsGL::get().queuebitmap(
                bitmap,
                load_class == DrawLoadClass::BLOCKING_VISIBLE
                    ? GraphicsGL::BitmapLoadClass::BLOCKING_VISIBLE
                    : GraphicsGL::BitmapLoadClass::TRANSIENT_EFFECT
            );
            return false;
#else
            GraphicsGL::get().addbitmap(bitmap);
#endif
        }

        GraphicsGL::get()
            .draw(bitmap, destination, args.get_color(), args.get_angle());
        return true;
    }

    void Texture::prepare_visible() const
    {
        if (!is_valid())
        {
            return;
        }

        GraphicsGL::get().queuebitmap(
            bitmap,
            GraphicsGL::BitmapLoadClass::BLOCKING_VISIBLE
        );
    }

    void Texture::prepare_effect() const
    {
        if (!is_valid())
        {
            return;
        }

        GraphicsGL::get().queuebitmap(
            bitmap,
            GraphicsGL::BitmapLoadClass::TRANSIENT_EFFECT
        );
    }

    void Texture::prepare_map_required() const
    {
        if (!is_valid())
        {
            return;
        }

        GraphicsGL::get().queuebitmap(
            bitmap,
            GraphicsGL::BitmapLoadClass::MAP_REQUIRED
        );
    }

    void Texture::shift(Point<int16_t> amount)
    {
        origin -= amount;
    }

    bool Texture::is_valid() const
    {
        return bitmap.id() > 0;
    }

    bool Texture::is_ready() const
    {
        return is_valid() && GraphicsGL::get().hasbitmap(bitmap);
    }

    int16_t Texture::width() const
    {
        return dimensions.x();
    }

    int16_t Texture::height() const
    {
        return dimensions.y();
    }

    Point<int16_t> Texture::get_origin() const
    {
        return origin;
    }

    Point<int16_t> Texture::get_dimensions() const
    {
        return dimensions;
    }
}
