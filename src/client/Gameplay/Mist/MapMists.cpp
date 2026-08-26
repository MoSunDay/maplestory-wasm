#include "MapMists.h"

#include "../../Graphics/GraphicsGL.h"

#include <algorithm>

namespace jrc
{
    void MapMists::draw(double viewx, double viewy) const
    {
        for (const auto& entry : mists)
        {
            const Mist& mist = entry.second;
            const int32_t left = std::min(mist.left, mist.right);
            const int32_t top = std::min(mist.top, mist.bottom);
            const int32_t width = std::abs(mist.right - mist.left);
            const int32_t height = std::abs(mist.bottom - mist.top);
            const float red = mist.type == 0 ? 0.45f : 0.2f;
            const float green = mist.type == 0 ? 0.15f : 0.55f;
            GraphicsGL::get().drawrectangle(
                static_cast<int16_t>(std::clamp<int32_t>(left + viewx, INT16_MIN, INT16_MAX)),
                static_cast<int16_t>(std::clamp<int32_t>(top + viewy, INT16_MIN, INT16_MAX)),
                static_cast<int16_t>(std::clamp<int32_t>(width, 0, INT16_MAX)),
                static_cast<int16_t>(std::clamp<int32_t>(height, 0, INT16_MAX)),
                red, green, 0.65f, 0.28f);
        }
    }

    void MapMists::spawn(int32_t oid, int32_t type, int32_t owner,
                         int32_t skill, uint8_t level, int32_t left,
                         int32_t top, int32_t right, int32_t bottom)
    {
        mists[oid] = { type, owner, skill, level, left, top, right, bottom };
    }

    void MapMists::remove(int32_t oid)
    {
        mists.erase(oid);
    }

    void MapMists::clear()
    {
        mists.clear();
    }

    size_t MapMists::size() const
    {
        return mists.size();
    }
}
