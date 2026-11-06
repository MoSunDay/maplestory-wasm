// Verifies the v83 FACE_EXPRESSION wire mapping used by local F-key emotes
// and by the remote FACIAL_EXPRESSION handler.
#include "Character/Look/ExpressionMapping.h"

#include <cstdint>
#include <iostream>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;

    // FACE1..FACE7 -> wire 1..7 -> face nodes HIT..STUNNED.
    for (int32_t action = 100; action <= 106; ++action)
    {
        const int32_t emote = jrc::faceemote::from_action(
            static_cast<size_t>(action));
        ok &= expect(emote == action - 99, "F-key action wire id");
        ok &= expect(
            jrc::faceemote::from_wire(emote)
                == static_cast<jrc::Expression::Id>(action - 98),
            "F-key action face expression");
    }

    ok &= expect(jrc::faceemote::from_action(99) == 0, "action below FACE1");
    ok &= expect(jrc::faceemote::from_action(107) == 0, "action above FACE7");

    ok &= expect(jrc::faceemote::from_wire(0) == jrc::Expression::DEFAULT,
        "wire 0 resets to default");

    const struct
    {
        int32_t emote;
        jrc::Expression::Id expression;
    } cash[] = {
        { 8, jrc::Expression::VOMIT },
        { 9, jrc::Expression::HUM },
        { 10, jrc::Expression::CHEERS },
        { 11, jrc::Expression::CHU },
        { 12, jrc::Expression::WINK },
        { 13, jrc::Expression::PAIN },
        { 14, jrc::Expression::SHINE },
        { 15, jrc::Expression::BLAZE },
        { 16, jrc::Expression::GLITTER },
        { 17, jrc::Expression::LOVE }
    };
    for (const auto& entry : cash)
    {
        ok &= expect(
            jrc::faceemote::from_wire(entry.emote) == entry.expression,
            "cash emote mapping");
    }

    ok &= expect(jrc::faceemote::from_wire(18) == jrc::Expression::DEFAULT,
        "unknown wire emote falls back to default");

    if (!ok)
        return 1;

    std::cout << "face-expression-sync: PASS\n";
    return 0;
}
