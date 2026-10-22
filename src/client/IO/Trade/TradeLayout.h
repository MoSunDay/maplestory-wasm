#pragma once

#include "../../Template/Rectangle.h"

#include <cstdint>

namespace jrc
{
    namespace trade_layout
    {
        constexpr int16_t WIDTH = 654;
        constexpr int16_t HEIGHT = 566;

        constexpr int16_t LOCAL_NAME_X = 85;
        constexpr int16_t PARTNER_NAME_X = 315;
        constexpr int16_t NAME_Y = 120;
        constexpr uint16_t NAME_WIDTH = 84;

        constexpr int16_t LOCAL_LOOK_X = 85;
        constexpr int16_t PARTNER_LOOK_X = 315;
        constexpr int16_t LOOK_Y = 108;

        constexpr int16_t LOCAL_ITEM_X = 49;
        constexpr int16_t PARTNER_ITEM_X = 242;
        constexpr int16_t ITEM_TOP = 151;
        constexpr int16_t ITEM_STEP = 40;
        constexpr int16_t ITEM_LABEL_DX = 40;
        constexpr int16_t ITEM_LABEL_DY = 8;
        constexpr uint16_t ITEM_LABEL_WIDTH = 100;

        constexpr int16_t LOCAL_MESO_X = 31;
        constexpr int16_t PARTNER_MESO_X = 224;
        constexpr int16_t MESO_Y = 364;

        constexpr int16_t LOCAL_CONFIRM_X = 85;
        constexpr int16_t PARTNER_CONFIRM_X = 315;
        constexpr int16_t CONFIRM_Y = 101;
        constexpr uint16_t CONFIRM_WIDTH = 120;

        constexpr int16_t CHAT_X = 410;
        constexpr int16_t CHAT_Y = 32;
        constexpr int16_t CHAT_STEP = 17;
        constexpr uint16_t CHAT_WIDTH = 226;

        constexpr int16_t PAGE_X = 388;
        constexpr int16_t PAGE_Y = 136;
        constexpr uint16_t PAGE_WIDTH = 80;

        inline constexpr Rectangle<int16_t> CHATFIELD{ 409, 578, 364, 378 };
    }
}
