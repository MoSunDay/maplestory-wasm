#pragma once

namespace jrc::bitmap_loading
{
    enum class LoadClass
    {
        BACKGROUND,
        MAP_REQUIRED,
        BLOCKING_VISIBLE,
        TRANSIENT_EFFECT
    };

    constexpr bool is_priority(LoadClass load_class)
    {
        return load_class != LoadClass::BACKGROUND;
    }

    constexpr bool blocks_gameplay(LoadClass load_class)
    {
        return load_class == LoadClass::BLOCKING_VISIBLE;
    }

    constexpr bool is_map_required(LoadClass load_class)
    {
        return load_class == LoadClass::MAP_REQUIRED ||
            load_class == LoadClass::BLOCKING_VISIBLE;
    }
}
