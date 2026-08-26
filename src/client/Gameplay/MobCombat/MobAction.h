#pragma once

#include "../../Template/Point.h"

#include <cstdint>

namespace jrc::mob_action
{
    enum class Shape : uint8_t
    {
        NONE,
        RECTANGLE,
        CIRCLE
    };

    struct Range
    {
        Shape shape = Shape::NONE;
        Point<int16_t> lt;
        Point<int16_t> rb;
        Point<int16_t> center;
        int16_t radius = 0;
    };

    struct Context
    {
        bool aggro = false;
        bool can_move = false;
        bool can_jump = false;
        bool can_fly = false;
        bool attack_ready = false;
        bool skill_ready = false;
        bool in_attack_range = false;
        bool in_skill_range = false;
        bool on_ground = false;
        int16_t delta_x = 0;
        int16_t delta_y = 0;
    };

    enum class Decision : uint8_t
    {
        IDLE,
        CHASE,
        JUMP,
        ATTACK,
        SKILL
    };

    Decision decide(const Context& context);
    bool contains(const Range& range, Point<int16_t> origin,
                  Point<int16_t> target, bool facing_right);
    uint8_t normal_activity(uint8_t attack_index, bool facing_right);
    uint8_t skill_activity(uint8_t action, bool facing_right);
}
