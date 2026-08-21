#include "FieldClockModel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace jrc::field_clock
{
    namespace
    {
        constexpr int32_t SECONDS_PER_DAY = 24 * 60 * 60;
    }

    int32_t display_seconds(Mode mode, int32_t initial_seconds, int64_t elapsed_seconds)
    {
        const int64_t elapsed = std::max<int64_t>(0, elapsed_seconds);
        switch (mode)
        {
        case Mode::WALL_CLOCK:
        {
            const int64_t normalized = std::max<int32_t>(0, initial_seconds) + elapsed;
            return static_cast<int32_t>(normalized % SECONDS_PER_DAY);
        }
        case Mode::COUNTDOWN:
            return static_cast<int32_t>(std::max<int64_t>(0, initial_seconds - elapsed));
        case Mode::INACTIVE:
            return 0;
        }

        return 0;
    }

    std::string format_seconds(int32_t seconds, bool always_show_hours)
    {
        const int32_t normalized = std::max<int32_t>(0, seconds);
        const int32_t hours = normalized / 3600;
        const int32_t minutes = (normalized / 60) % 60;
        const int32_t remaining_seconds = normalized % 60;

        std::ostringstream stream;
        stream << std::setfill('0');
        if (always_show_hours || hours > 0)
        {
            stream << std::setw(2) << hours << ':';
        }
        stream << std::setw(2) << minutes << ':'
               << std::setw(2) << remaining_seconds;
        return stream.str();
    }
}
