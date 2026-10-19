//////////////////////////////////////////////////////////////////////////////
// Pure layout math for the character select screen.
//
// The shipped post-Chaos UI.nx arranges the roster as a single row of
// four small disc platforms with buyCharacter purchase cards and page
// arrows left of the info board. These constants and free functions keep
// those anchors in one place; art specific offsets that only apply to the
// pre-Chaos (v83) fallback stay at their call sites.
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Template/Point.h"

#include <cstddef>
#include <cstdint>

namespace jrc
{
    namespace charselect_layout
    {
        // Characters per page: the post-Chaos roster is a single row.
        constexpr uint8_t PAGESIZE = 4;

        // character/0 is a 62x8 disc with origin (29,7), so drawing it at
        // the feet anchor sinks the feet into the art. Shifting the disc
        // down puts its top face at feet level; the disc body then spans
        // charpos.y..charpos.y+8. The name tag (86x16, origin (36,8),
        // anchored at its vertical center) must clear that body:
        // offset 19 puts the tag top 3px below the disc's lower rim
        // instead of burying its upper half in the disc art.
        constexpr Point<int16_t> PLATFORM_OFFSET{ 0, 7 };
        constexpr Point<int16_t> NAMETAG_OFFSET{ 0, 19 };

        // The arrows flank the roster row: the right one squeezes
        // between the widest buyCharacter frame (right edge ~531) and
        // the info board's opaque left border (~573).
        constexpr Point<int16_t> PAGE_ARROW_LEFT_POS{ 46, 240 };
        constexpr Point<int16_t> PAGE_ARROW_RIGHT_POS{ 533, 240 };

        // Selected character data block origin, relative to the info
        // board anchor.
        constexpr Point<int16_t> CHARACTER_DATA_OFFSET{ -3, -6 };

        // Info board label slots, in UICharSelect::InfoLabel order.
        enum Label
        {
            LABEL_JOB,
            LABEL_STR,
            LABEL_INT,
            LABEL_DEX,
            LABEL_LUK
        };

        // Feet anchor of slot i in the single roster row.
        constexpr Point<int16_t> char_slot_pos(size_t i)
        {
            return { static_cast<int16_t>(130 + 120 * i), 270 };
        }

        constexpr Point<int16_t> slot_hotzone_pos(size_t i)
        {
            return { static_cast<int16_t>(100 + 120 * i), 160 };
        }

        constexpr Point<int16_t> slot_hotzone_dim()
        {
            return { 60, 120 };
        }

        // Offset of an info board label relative to the character data
        // origin. The post-Chaos charInfo board puts the job above the
        // 2x2 stat table; on the pre-Chaos charInfo1 board the job goes
        // on its own right aligned line and the stats fill the lower
        // half.
        constexpr Point<int16_t> label_pos(size_t label, bool post_chaos)
        {
            if (!post_chaos)
            {
                switch (label)
                {
                case LABEL_JOB:
                    return { 113, 31 };
                case LABEL_STR:
                    return { 38, 56 };
                case LABEL_INT:
                    return { 113, 56 };
                case LABEL_DEX:
                    return { 38, 81 };
                case LABEL_LUK:
                    return { 113, 81 };
                }
            }

            switch (label)
            {
            case LABEL_JOB:
                return { 63, -39 };
            case LABEL_STR:
                return { -8, -12 };
            case LABEL_INT:
                return { 63, -12 };
            case LABEL_DEX:
                return { -8, 13 };
            case LABEL_LUK:
                return { 63, 13 };
            }

            return {};
        }
    }
}
