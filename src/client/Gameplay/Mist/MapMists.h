#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace jrc
{
    class MapMists
    {
    public:
        void draw(double viewx, double viewy) const;
        void spawn(int32_t oid, int32_t type, int32_t owner, int32_t skill,
                   uint8_t level, int32_t left, int32_t top,
                   int32_t right, int32_t bottom);
        void remove(int32_t oid);
        void clear();
        size_t size() const;

    private:
        struct Mist
        {
            int32_t type;
            int32_t owner;
            int32_t skill;
            uint8_t level;
            int32_t left;
            int32_t top;
            int32_t right;
            int32_t bottom;
        };

        std::unordered_map<int32_t, Mist> mists;
    };
}
