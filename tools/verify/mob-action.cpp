#include "Gameplay/MobCombat/MobAction.h"

#include <cstdlib>
#include <iostream>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    using namespace jrc;
    using namespace jrc::mob_action;

    Context context;
    require(decide(context) == Decision::IDLE, "non-aggro mob remains idle");

    context.aggro = true;
    context.can_move = true;
    context.attack_ready = true;
    context.in_attack_range = true;
    require(decide(context) == Decision::ATTACK, "in-range normal attack is selected");

    context.skill_ready = true;
    context.in_skill_range = true;
    require(decide(context) == Decision::SKILL, "eligible suggested skill has priority");

    context.skill_ready = false;
    context.in_skill_range = false;
    context.attack_ready = false;
    context.in_attack_range = false;
    context.can_jump = true;
    context.on_ground = true;
    context.delta_y = -46;
    require(decide(context) == Decision::JUMP, "ground mob jumps to reachable target");

    context.delta_y = 0;
    require(decide(context) == Decision::CHASE, "aggro mob chases out-of-range target");

    Range rectangle;
    rectangle.shape = Shape::RECTANGLE;
    rectangle.lt = Point<int16_t>(10, -20);
    rectangle.rb = Point<int16_t>(80, 20);
    require(contains(rectangle, {100, 100}, {150, 100}, true),
            "right-facing rectangle contains target");
    require(!contains(rectangle, {100, 100}, {50, 100}, true),
            "right-facing rectangle excludes rear target");
    require(contains(rectangle, {100, 100}, {50, 100}, false),
            "left-facing rectangle mirrors target space");

    Range circle;
    circle.shape = Shape::CIRCLE;
    circle.center = Point<int16_t>(30, 0);
    circle.radius = 20;
    require(contains(circle, {0, 0}, {50, 0}, true), "circle includes boundary");
    require(!contains(circle, {0, 0}, {51, 0}, true), "circle excludes outside point");

    require(normal_activity(0, true) == 24, "attack1 right activity");
    require(normal_activity(8, false) == 41, "attack9 left activity");
    require(skill_activity(1, true) == 42, "skill action1 right activity");
    require(skill_activity(9, false) == 59, "skill action9 left activity");

    std::cout << "mob action verification passed\n";
}
