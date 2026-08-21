#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>

#include "Graphics/Animation/FrameProperties.h"

int main()
{
    using jrc::animation::opacity_endpoints;

    assert((opacity_endpoints(std::nullopt, std::nullopt) ==
        std::pair<uint8_t, uint8_t>{ 255, 255 }));
    assert((opacity_endpoints(uint8_t{ 128 }, std::nullopt) ==
        std::pair<uint8_t, uint8_t>{ 128, 128 }));
    assert((opacity_endpoints(std::nullopt, uint8_t{ 0 }) ==
        std::pair<uint8_t, uint8_t>{ 255, 0 }));
    assert((opacity_endpoints(uint8_t{ 64 }, uint8_t{ 192 }) ==
        std::pair<uint8_t, uint8_t>{ 64, 192 }));
}
