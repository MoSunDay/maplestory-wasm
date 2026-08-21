#include "client/IO/Field/FieldClockModel.h"

#include <iostream>

int main()
{
    using jrc::field_clock::Mode;
    using jrc::field_clock::display_seconds;
    using jrc::field_clock::format_seconds;

    const bool ok =
        display_seconds(Mode::COUNTDOWN, 1800, 1) == 1799 &&
        display_seconds(Mode::COUNTDOWN, 2, 10) == 0 &&
        display_seconds(Mode::WALL_CLOCK, 23 * 3600 + 59 * 60 + 59, 2) == 1 &&
        display_seconds(Mode::INACTIVE, 500, 0) == 0 &&
        format_seconds(1800, false) == "30:00" &&
        format_seconds(3661, false) == "01:01:01" &&
        format_seconds(1, true) == "00:00:01";

    if (!ok)
    {
        std::cerr << "FAIL field clock countdown, wrap, or formatting contract\n";
        return 1;
    }

    std::cout << "PASS field clock countdown, wrap, and formatting contract\n";
    return 0;
}
