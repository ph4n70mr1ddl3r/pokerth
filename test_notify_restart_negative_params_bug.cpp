/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

// Regression test: IRC admin "notify-restart" accepts negative parameters and
// immediately broadcasts a false "The server will be restarted NOW." to every
// player via global chat and a global message box.
//
// Background
// ----------
// src/net/serveradminbot.cpp, ServerAdminBot::SignalIrcChatMsg():
//
//   line 249:  msgStream >> timeoutMinutes;
//   line 250:  msgStream >> intervalMinutes;
//   line 251:  if (timeoutMinutes && intervalMinutes) {
//   ...
//   line 256:      m_notifyTimeoutMinutes = timeoutMinutes;
//   line 257:      m_notifyIntervalMinutes = intervalMinutes;
//
// Both parameters are plain `int`. The truthiness check accepts any non-zero
// value, including negatives. An admin typo such as
// `BotNick: notify-restart -5 -3` is therefore accepted and acknowledged with
// "Restart planned in -5 minutes, notifying every -3 minutes."
//
// ServerAdminBot::NotifyLoop() (lines 381-421) then computes, on the very first
// ~1 second tick (m_notifyCounter == 0):
//
//   outer guard:  elapsed >= 0 * (-3) * 60  -> 1 >= 0   -> true
//   remaining:    -5 - 0 * (-3)             -> -5
//   NOW branch:   remaining <= 0 && elapsed >= -5 * 60  -> true
//
// so the loop sends the "restart NOW" global chat + message box on the first
// tick. A negative interval alone also makes the outer guard permanently true,
// spamming restart notices every tick.
//
// The fix
// -------
// Require strictly positive values: `if (timeoutMinutes > 0 && intervalMinutes > 0)`.
// Non-numeric / missing arguments leave the ints at 0 and are already rejected.
//
// This standalone test models both the parameter validation and one NotifyLoop
// tick, and asserts that negative parameters are rejected and can never emit a
// "restart NOW" broadcast after the fix.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_notify_restart_negative_params_bug test_notify_restart_negative_params_bug.cpp
//   /tmp/test_notify_restart_negative_params_bug

#include <cstdio>

static int failures = 0;

static void check(const char *label, bool ok)
{
    std::printf("  %-62s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

// Parameter validation as written before the fix (serveradminbot.cpp:251).
static bool acceptsParamsBuggy(int timeoutMinutes, int intervalMinutes)
{
    return timeoutMinutes && intervalMinutes;
}

// Parameter validation after the fix.
static bool acceptsParamsFixed(int timeoutMinutes, int intervalMinutes)
{
    return timeoutMinutes > 0 && intervalMinutes > 0;
}

struct NotifyState {
    int timeoutMinutes;
    int intervalMinutes;
    long long elapsedSeconds;
    int counter;
};

// Mirrors one ServerAdminBot::NotifyLoop() tick. Returns true when the
// "The server will be restarted NOW." broadcast is emitted.
static bool notifyTick(NotifyState &s)
{
    if (s.timeoutMinutes &&
        s.elapsedSeconds >= static_cast<long long>(s.counter) * s.intervalMinutes * 60) {
        const int remainingMinutes = static_cast<int>(
            s.timeoutMinutes - static_cast<long long>(s.counter) * s.intervalMinutes);
        ++s.counter;
        if (remainingMinutes <= 0 &&
            s.elapsedSeconds >= static_cast<long long>(s.timeoutMinutes) * 60) {
            return true;
        }
    }
    return false;
}

static void runCase(const char *name, int timeoutMinutes, int intervalMinutes, bool expectNowBuggy)
{
    std::printf("\n%s: timeout=%d, interval=%d\n", name, timeoutMinutes, intervalMinutes);

    const bool buggyAccepted = acceptsParamsBuggy(timeoutMinutes, intervalMinutes);
    const bool fixedAccepted = acceptsParamsFixed(timeoutMinutes, intervalMinutes);

    NotifyState buggyState{timeoutMinutes, intervalMinutes, 1, 0};
    const bool buggyNow = buggyAccepted && notifyTick(buggyState);

    NotifyState fixedState{timeoutMinutes, intervalMinutes, 1, 0};
    const bool fixedNow = fixedAccepted && notifyTick(fixedState);

    std::printf("  buggy: accepted=%d, firstTickNOW=%d\n", buggyAccepted, buggyNow);
    std::printf("  fixed: accepted=%d, firstTickNOW=%d\n", fixedAccepted, fixedNow);

    if (timeoutMinutes < 0 || intervalMinutes < 0) {
        check("buggy accepts negative parameter (bug reproduced)", buggyAccepted);
        check("fixed rejects negative parameter", !fixedAccepted);
        check("fixed never emits 'restart NOW'", !fixedNow);
        check("buggy emits 'restart NOW' on first tick", buggyNow == expectNowBuggy);
    } else if (timeoutMinutes == 0 || intervalMinutes == 0) {
        check("both reject zero parameter", !buggyAccepted && !fixedAccepted);
        check("neither emits 'restart NOW'", !buggyNow && !fixedNow);
    } else {
        check("both accept valid parameters", buggyAccepted && fixedAccepted);
        check("valid parameters do not emit 'restart NOW' on first tick",
              !buggyNow && !fixedNow);
        check("buggy and fixed behave identically for valid input",
              buggyNow == fixedNow && buggyAccepted == fixedAccepted);
    }
}

int main()
{
    std::printf("Testing notify-restart negative parameter bug (false 'restart NOW' broadcast):\n");

    runCase("negative timeout and interval", -5, -3, true);
    runCase("negative timeout only", -5, 3, true);
    runCase("negative interval only", 10, -3, false);
    runCase("zero interval", 5, 0, false);
    runCase("zero timeout", 0, 5, false);
    runCase("valid parameters", 10, 3, false);

    if (failures != 0) {
        std::printf("\n%d CHECK(S) FAILED\n", failures);
        return 1;
    }
    std::printf("\nALL CHECKS PASSED\n");
    return 0;
}
