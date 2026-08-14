#include <cassert>

#include "Graphics/BitmapLoadClass.h"

int main()
{
    using jrc::bitmap_loading::LoadClass;
    using jrc::bitmap_loading::blocks_gameplay;
    using jrc::bitmap_loading::is_map_required;
    using jrc::bitmap_loading::is_priority;

    assert(!is_priority(LoadClass::BACKGROUND));
    assert(!blocks_gameplay(LoadClass::BACKGROUND));
    assert(!is_map_required(LoadClass::BACKGROUND));

    assert(is_priority(LoadClass::MAP_REQUIRED));
    assert(!blocks_gameplay(LoadClass::MAP_REQUIRED));
    assert(is_map_required(LoadClass::MAP_REQUIRED));

    assert(is_priority(LoadClass::BLOCKING_VISIBLE));
    assert(blocks_gameplay(LoadClass::BLOCKING_VISIBLE));
    assert(is_map_required(LoadClass::BLOCKING_VISIBLE));

    assert(is_priority(LoadClass::TRANSIENT_EFFECT));
    assert(!blocks_gameplay(LoadClass::TRANSIENT_EFFECT));
    assert(!is_map_required(LoadClass::TRANSIENT_EFFECT));
}
