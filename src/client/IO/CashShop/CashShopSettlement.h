#pragma once

#include <chrono>
#include <cstdint>

namespace jrc
{
    namespace cash_shop_settlement
    {
        // One pending outgoing operation at a time. EXIT is tracked here too
        // so a lost SetField reply can be recovered without guessing from the
        // operation response opcodes.
        enum PendingKind : uint8_t
        {
            NONE = 0,
            BUY = 1,
            TAKE = 2,
            PUT = 3,
            EXIT = 4
        };

        using Clock = std::chrono::steady_clock;

        // Wall-clock budgets. The grace period absorbs one modal transition or
        // a frame hitch without turning a slow-but-live reply into an error.
        inline constexpr auto BALANCE_TIMEOUT = std::chrono::seconds(10);
        inline constexpr auto OPERATION_TIMEOUT = std::chrono::seconds(10);
        inline constexpr auto EXIT_TIMEOUT = std::chrono::seconds(8);
        inline constexpr auto SETTLE_GRACE = std::chrono::milliseconds(500);

        constexpr bool is_operation(PendingKind kind)
        {
            return kind == BUY || kind == TAKE || kind == PUT;
        }

        constexpr bool can_mutate(bool balances_ready, bool balance_failed,
            bool pending, bool require_reenter)
        {
            return balances_ready && !balance_failed && !pending &&
                !require_reenter;
        }

        constexpr bool can_exit(bool pending)
        {
            return !pending;
        }

        constexpr auto timeout_for(PendingKind kind)
        {
            return kind == EXIT ? EXIT_TIMEOUT : OPERATION_TIMEOUT;
        }

        constexpr PendingKind response_kind(uint8_t opcode)
        {
            switch (opcode)
            {
            case 0x57:
            case 0x89:
                return BUY;
            case 0x68:
                return TAKE;
            case 0x6A:
                return PUT;
            default:
                return NONE;
            }
        }

        constexpr bool success_matches(PendingKind pending, uint8_t opcode)
        {
            return is_operation(pending) &&
                pending == response_kind(opcode);
        }

        template <typename TimePoint>
        constexpr bool deadline_reached(TimePoint now, TimePoint deadline)
        {
            return now >= deadline;
        }
    }
}
