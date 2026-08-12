#pragma once

#include "../KeyConfig.h"
#include "../../Template/Rectangle.h"

#include <map>

namespace jrc::KeyConfigLayout
{
    using KeyBounds = std::map<KeyConfig::Key, Rectangle<int16_t>>;

    // Returns the interactive key rectangles painted by
    // UIWindow2.img/KeyConfig/backgrnd3.
    KeyBounds key_bounds();
}
