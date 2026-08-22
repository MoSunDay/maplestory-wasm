#pragma once

#include <cstdint>

namespace jrc::pet_settings
{
    struct Thresholds
    {
        uint8_t hp = 50;
        uint8_t mp = 50;
    };

    Thresholds load();
    bool save(const Thresholds& thresholds);
}
