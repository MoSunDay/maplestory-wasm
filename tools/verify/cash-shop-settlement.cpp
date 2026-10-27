// c++ -std=c++17 -Isrc/client tools/verify/cash-shop-settlement.cpp -o /tmp/cash-shop-settlement && /tmp/cash-shop-settlement
// Pure native checks for IO/CashShop/CashShopSettlement.h: the pending-operation
// state machine has no NX/server/browser dependencies, so it is verified here.
#include "IO/CashShop/CashShopSettlement.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace cs = jrc::cash_shop_settlement;

namespace
{
    bool ok = true;

    void check(bool condition, const std::string& message)
    {
        if (condition)
        {
            std::cout << "OK " << message << '\n';
        }
        else
        {
            ok = false;
            std::cerr << "FAIL " << message << '\n';
        }
    }

    const char* name(cs::PendingKind kind)
    {
        switch (kind)
        {
        case cs::NONE: return "NONE";
        case cs::BUY: return "BUY";
        case cs::TAKE: return "TAKE";
        case cs::PUT: return "PUT";
        case cs::EXIT: return "EXIT";
        }
        return "?";
    }

    std::string flags(bool ready, bool failed, bool pending, bool reenter)
    {
        return std::string("ready=") + (ready ? "1" : "0") +
            " failed=" + (failed ? "1" : "0") +
            " pending=" + (pending ? "1" : "0") +
            " reenter=" + (reenter ? "1" : "0");
    }

    std::string hex(uint8_t opcode)
    {
        const char* digits = "0123456789abcdef";
        std::string text = "0x";
        text += digits[(opcode >> 4) & 0xF];
        text += digits[opcode & 0xF];
        return text;
    }

    // Independent copy of the wire contract: which reply opcode acknowledges
    // which pending operation. Kept separate from response_kind() on purpose so
    // a regression in the header shows up as a failure instead of a tautology.
    bool is_success_opcode(cs::PendingKind pending, uint8_t opcode)
    {
        switch (pending)
        {
        case cs::BUY: return opcode == 0x57 || opcode == 0x89;
        case cs::TAKE: return opcode == 0x68;
        case cs::PUT: return opcode == 0x6A;
        case cs::NONE:
        case cs::EXIT:
            return false;
        }
        return false;
    }

    constexpr cs::PendingKind kinds[] =
    {
        cs::NONE, cs::BUY, cs::TAKE, cs::PUT, cs::EXIT
    };
}

// The header is deliberately constexpr: these prove the helpers stay usable in
// constant expressions (the client relies on that for table-driven checks).
static_assert(cs::is_operation(cs::BUY), "BUY is an operation");
static_assert(!cs::is_operation(cs::EXIT), "EXIT is not an operation");
static_assert(cs::response_kind(0x89) == cs::BUY, "0x89 acknowledges BUY");
static_assert(cs::success_matches(cs::TAKE, 0x68), "0x68 settles TAKE");
static_assert(!cs::success_matches(cs::NONE, 0x68), "NONE never settles");
static_assert(cs::timeout_for(cs::EXIT) == cs::EXIT_TIMEOUT, "EXIT uses its own budget");
static_assert(cs::timeout_for(cs::BUY) == cs::OPERATION_TIMEOUT, "operations share a budget");
static_assert(cs::SETTLE_GRACE == std::chrono::milliseconds(500), "grace is 500ms");

int main()
{
    // ---- can_mutate: full 4-bit truth table -------------------------------
    // bit0=balances_ready, bit1=balance_failed, bit2=pending, bit3=require_reenter.
    // Only the all-clear combination (bit0 set, everything else clear) may pass.
    for (int bits = 0; bits < 16; bits += 1)
    {
        const bool ready = (bits & 1) != 0;
        const bool failed = (bits & 2) != 0;
        const bool pending = (bits & 4) != 0;
        const bool reenter = (bits & 8) != 0;
        const bool expected = bits == 1;
        check(cs::can_mutate(ready, failed, pending, reenter) == expected,
            "can_mutate " + flags(ready, failed, pending, reenter) +
            " -> " + (expected ? "true" : "false"));
    }
    check(!cs::can_mutate(false, false, false, false),
        "can_mutate stays closed until balances are ready");

    // ---- can_exit ---------------------------------------------------------
    check(cs::can_exit(false), "can_exit true with no pending operation");
    check(!cs::can_exit(true), "can_exit false while an operation is pending");

    // ---- is_operation -----------------------------------------------------
    for (const auto kind : kinds)
    {
        const bool expected =
            kind == cs::BUY || kind == cs::TAKE || kind == cs::PUT;
        check(cs::is_operation(kind) == expected,
            std::string("is_operation ") + name(kind) +
            " -> " + (expected ? "true" : "false"));
    }

    // ---- timeout_for ------------------------------------------------------
    check(cs::timeout_for(cs::EXIT) == cs::EXIT_TIMEOUT,
        "timeout_for EXIT == EXIT_TIMEOUT");
    check(std::chrono::duration_cast<std::chrono::milliseconds>(
        cs::timeout_for(cs::EXIT)).count() == 8000,
        "EXIT budget is 8000ms");
    for (const auto kind : kinds)
    {
        if (kind == cs::EXIT) continue;
        check(cs::timeout_for(kind) == cs::OPERATION_TIMEOUT,
            std::string("timeout_for ") + name(kind) + " == OPERATION_TIMEOUT");
    }
    check(std::chrono::duration_cast<std::chrono::milliseconds>(
        cs::OPERATION_TIMEOUT).count() == 10000, "operation budget is 10000ms");
    check(std::chrono::duration_cast<std::chrono::milliseconds>(
        cs::BALANCE_TIMEOUT).count() == 10000, "balance budget is 10000ms");
    check(std::chrono::duration_cast<std::chrono::milliseconds>(
        cs::SETTLE_GRACE).count() == 500, "settle grace is 500ms");
    check(cs::EXIT_TIMEOUT < cs::OPERATION_TIMEOUT,
        "EXIT budget is shorter than an operation budget");

    // ---- response_kind ----------------------------------------------------
    {
        const struct { uint8_t opcode; cs::PendingKind expected; } cases[] =
        {
            { 0x57, cs::BUY }, { 0x89, cs::BUY },
            { 0x68, cs::TAKE }, { 0x6A, cs::PUT },
            { 0x5C, cs::NONE }, { 0x00, cs::NONE }, { 0xFF, cs::NONE },
            // 0x144 does not fit the byte channel; the client truncates it to
            // 0x44, which is not an acknowledgement.
            { static_cast<uint8_t>(0x144), cs::NONE },
        };
        for (const auto& probe : cases)
        {
            check(cs::response_kind(probe.opcode) == probe.expected,
                "response_kind " + hex(probe.opcode) + " -> " + name(probe.expected));
        }
    }
    {
        // Exhaustive sweep: exactly two BUY codes, one TAKE, one PUT, rest NONE.
        int buys = 0, takes = 0, puts = 0, none = 0;
        for (int value = 0; value < 256; value += 1)
        {
            switch (cs::response_kind(static_cast<uint8_t>(value)))
            {
            case cs::BUY: buys += 1; break;
            case cs::TAKE: takes += 1; break;
            case cs::PUT: puts += 1; break;
            case cs::NONE: none += 1; break;
            case cs::EXIT: none = -1000; break;
            }
        }
        check(buys == 2 && takes == 1 && puts == 1 && none == 252,
            "response_kind sweep counts buy=2 take=1 put=1 none=252 (got " +
            std::to_string(buys) + "/" + std::to_string(takes) + "/" +
            std::to_string(puts) + "/" + std::to_string(none) + ")");
    }

    // ---- success_matches --------------------------------------------------
    {
        const uint8_t opcodes[] = { 0x57, 0x89, 0x68, 0x6A, 0x5C, 0x00 };
        for (const auto kind : kinds)
        {
            for (const auto opcode : opcodes)
            {
                const bool expected = is_success_opcode(kind, opcode);
                check(cs::success_matches(kind, opcode) == expected,
                    std::string("success_matches ") + name(kind) + " " +
                    hex(opcode) + " -> " + (expected ? "true" : "false"));
            }
        }
        int mismatches = 0;
        for (const auto kind : kinds)
        {
            for (int value = 0; value < 256; value += 1)
            {
                const auto opcode = static_cast<uint8_t>(value);
                if (cs::success_matches(kind, opcode) !=
                    is_success_opcode(kind, opcode))
                    mismatches += 1;
            }
        }
        check(mismatches == 0,
            "success_matches sweep agrees on every kind x opcode (mismatches=" +
            std::to_string(mismatches) + ")");
        check(!cs::success_matches(cs::NONE, 0x57) &&
            !cs::success_matches(cs::EXIT, 0x57),
            "NONE/EXIT never accept a BUY acknowledgement");
    }

    // ---- deadline_reached boundaries --------------------------------------
    {
        const cs::Clock::time_point start =
            cs::Clock::time_point{} + std::chrono::milliseconds(1000);
        const cs::Clock::time_point deadline =
            start + std::chrono::milliseconds(500);
        check(!cs::deadline_reached(start + std::chrono::milliseconds(499), deadline),
            "deadline_reached false 1ms before the deadline");
        check(cs::deadline_reached(deadline, deadline),
            "deadline_reached true exactly at the deadline");
        check(cs::deadline_reached(start + std::chrono::milliseconds(501), deadline),
            "deadline_reached true 1ms after the deadline");
        check(cs::deadline_reached(start, start),
            "deadline_reached true for a zero-length deadline");
        // The grace window is applied by subtracting it from the deadline; a
        // moment strictly inside the budget must still be alive.
        const cs::Clock::time_point operation_deadline =
            start + std::chrono::milliseconds(8000) - cs::SETTLE_GRACE;
        check(!cs::deadline_reached(start + std::chrono::milliseconds(7499),
            operation_deadline), "grace-adjusted deadline keeps 7499ms alive");
        check(cs::deadline_reached(start + std::chrono::milliseconds(7500),
            operation_deadline), "grace-adjusted deadline expires at 7500ms");
        const cs::Clock::time_point now = cs::Clock::now();
        check(!cs::deadline_reached(now, now + cs::BALANCE_TIMEOUT),
            "live clock is inside the balance budget");
        check(cs::deadline_reached(now, now - cs::SETTLE_GRACE),
            "live clock is past an expired grace window");
    }

    if (ok)
        std::cout << "PASS cash shop settlement state machine\n";
    return ok ? 0 : 1;
}
