// Guards src/client/IO/DefaultKeymap.h against drifting away from the
// official v83 keyset the game server installs on character creation
// (GameConstants DEFAULT_KEY/DEFAULT_TYPE/DEFAULT_ACTION in
// link_repos/MapleStory-Server). Run from the repo root:
//   c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client tools/verify/default-keymap.cpp -o /tmp/default-keymap && /tmp/default-keymap

#include "IO/DefaultKeymap.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

namespace
{
    struct Binding
    {
        int key;
        int type;
        int action;
    };

    // link_repos/MapleStory-Server/src/constants/GameConstants.java
    const std::vector<Binding> OFFICIAL_V83 = {
        {18, 4, 0},    {65, 6, 106}, {2, 4, 10},  {23, 4, 1},   {3, 4, 12},
        {4, 4, 13},    {5, 4, 18},   {6, 4, 24},  {16, 4, 8},   {17, 4, 5},
        {19, 4, 4},    {25, 4, 19},  {26, 4, 14}, {27, 4, 15},  {31, 4, 2},
        {34, 4, 17},   {35, 4, 11},  {37, 4, 3},  {38, 4, 20},  {40, 4, 16},
        {43, 4, 9},    {44, 5, 50},  {45, 5, 51}, {46, 4, 6},   {50, 4, 7},
        {56, 5, 53},   {59, 6, 100}, {60, 6, 101}, {61, 6, 102}, {62, 6, 103},
        {63, 6, 104},  {64, 6, 105}, {57, 5, 54}, {48, 4, 22},  {29, 5, 52},
        {7, 4, 21},    {24, 4, 25},  {33, 4, 26}, {41, 4, 23},  {39, 4, 27},
    };

    // Client-side conveniences that extend (never displace) the official set.
    const std::vector<Binding> ALLOWED_EXTRAS = {
        {85, 5, 52}, // RIGHT_CONTROL -> ATTACK
        {87, 5, 53}, // RIGHT_ALT -> JUMP
    };

    std::map<int, std::pair<int, int>> to_map(const std::vector<Binding>& rows)
    {
        std::map<int, std::pair<int, int>> out;
        for (const Binding& row : rows)
            out[row.key] = {row.type, row.action};
        return out;
    }
}

int main()
{
    const std::map<int32_t, jrc::Keyboard::Mapping> client =
        jrc::default_basic_mappings();
    const std::map<int, std::pair<int, int>> official = to_map(OFFICIAL_V83);
    const std::map<int, std::pair<int, int>> extras = to_map(ALLOWED_EXTRAS);

    int failures = 0;

    for (const auto& kv : official)
    {
        auto it = client.find(kv.first);
        if (it == client.end())
        {
            std::printf("MISSING official key %d (type %d action %d)\n",
                        kv.first, kv.second.first, kv.second.second);
            ++failures;
        }
        else if (it->second.type != kv.second.first
                 || it->second.action != kv.second.second)
        {
            std::printf(
                "WRONG key %d: got type %d action %d, want type %d action %d\n",
                kv.first, it->second.type, it->second.action,
                kv.second.first, kv.second.second);
            ++failures;
        }
    }

    for (const auto& kv : client)
    {
        if (official.count(kv.first) == 0 && extras.count(kv.first) == 0)
        {
            std::printf("UNEXPECTED key %d (type %d action %d) beyond the"
                        " official v83 set and the allowed extras\n",
                        kv.first, kv.second.type, kv.second.action);
            ++failures;
        }
    }

    if (failures > 0)
    {
        std::printf("%d default keymap mismatch(es)\n", failures);
        return EXIT_FAILURE;
    }

    std::printf("default keymap matches the official v83 keyset (%zu official"
                " entries, %zu allowed extras)\n",
                official.size(), extras.size());
    return EXIT_SUCCESS;
}
