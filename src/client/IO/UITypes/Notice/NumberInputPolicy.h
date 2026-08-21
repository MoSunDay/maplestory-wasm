#pragma once

#include <cstdint>
#include <string>

namespace jrc::number_input_policy
{
    inline std::string clamp_maximum(const std::string& value, int32_t maximum)
    {
        if (value.empty() || maximum < 0)
        {
            return value;
        }

        int64_t parsed = 0;
        for (unsigned char character : value)
        {
            if (character < '0' || character > '9')
            {
                return value;
            }

            parsed = parsed * 10 + (character - '0');
            if (parsed > maximum)
            {
                return std::to_string(maximum);
            }
        }

        return value;
    }
}
