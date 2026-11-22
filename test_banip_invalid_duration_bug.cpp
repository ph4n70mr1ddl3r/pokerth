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

// Regression test: a malformed duration in the IRC admin "banip" command
// silently turns the 12-hour default ban into a PERMANENT ban.
//
// Background
// ----------
// ServerAdminBot::SignalIrcChatMsg() parses "banip" as follows
// (src/net/serveradminbot.cpp:143-158):
//
//     string ipAddress;
//     unsigned durationHours = 12;
//     msgStream >> ipAddress;
//     while (msgStream.peek() == ' ')
//         msgStream.get();
//     if (!msgStream.eof())
//         msgStream >> durationHours;
//     ...
//     GetLobbyThread().GetBanManager().BanIPAddress(ipAddress, durationHours);
//
// The bug
// -------
// Since C++11, a failed integer extraction writes 0 to the target (the
// num_get facet zeroes the value before parsing). So for a command like
//
//     BotName: banip 1.2.3.4 abc
//
// the `msgStream >> durationHours` extraction fails on 'a' and overwrites the
// documented 12-hour default with 0. ServerBanManager::InternalRegisterTimedBan()
// (src/net/serverbanmanager.cpp:271-280) treats 0 as "no timer", i.e. a
// PERMANENT ban:
//
//     if (durationHours) {
//         tmpTimer = ...;  // timed ban only for a non-zero duration
//     }
//
// A single typo in the optional duration therefore upgrades a 12-hour ban to a
// permanent one, and the bot's own reply claims the IP "was added ... for 0
// hours". The documented command form ("banip 1.2.3.4", no duration) is
// unaffected, which is why this slipped through.
//
// The fix
// -------
// Read the optional duration as a token and accept it only when the ENTIRE
// token is a valid non-negative decimal number (std::from_chars with a
// full-consumption check). Otherwise keep the 12-hour default. This also
// rejects "-5" (which the old extraction silently wrapped to ~4 billion
// hours, also effectively a permanent ban) and "4.5" (which it truncated to
// 4). An explicitly supplied 0 is still honoured so no existing intentional
// behaviour changes.
//
// This standalone test uses a real std::istringstream and std::from_chars, so
// the parsing semantics are exercised by the compiler/standard library rather
// than assumed. No Qt, Boost or server objects are required.

#include <charconv>
#include <cstdio>
#include <sstream>
#include <string>
#include <system_error>

using std::istringstream;
using std::string;

static const unsigned DEFAULT_DURATION_HOURS = 12;

struct ParseResult {
    string ipAddress;
    unsigned durationHours = 0;
};

// ---------- model of the buggy parser (serveradminbot.cpp:143-158) ----------

static ParseResult parseBuggy(const string &args)
{
    istringstream msgStream(args);
    ParseResult result;
    msgStream >> result.ipAddress;
    while (msgStream.peek() == ' ')
        msgStream.get();
    if (!msgStream.eof())
        msgStream >> result.durationHours;
    return result;
}

// ---------- model of the fixed parser ----------

static ParseResult parseFixed(const string &args)
{
    istringstream msgStream(args);
    ParseResult result;
    result.durationHours = DEFAULT_DURATION_HOURS;
    msgStream >> result.ipAddress;
    while (msgStream.peek() == ' ')
        msgStream.get();
    if (!msgStream.eof()) {
        string durationToken;
        if (msgStream >> durationToken) {
            unsigned parsedDurationHours = 0;
            const char *begin = durationToken.data();
            const char *end = begin + durationToken.size();
            auto [ptr, ec] = std::from_chars(begin, end, parsedDurationHours);
            if (ec == std::errc() && ptr == end) {
                result.durationHours = parsedDurationHours;
            }
        }
    }
    return result;
}

// ServerBanManager::InternalRegisterTimedBan(): duration 0 means "no timer",
// which is a permanent ban. A non-zero duration is a timed ban.
static bool isPermanentBan(unsigned durationHours)
{
    return durationHours == 0;
}

// ---------- test harness ----------

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (condition) {
        printf("PASS: %s\n", message);
    } else {
        printf("FAIL: %s\n", message);
        ++failures;
    }
}

int main()
{
    printf("Testing banip malformed-duration permanent-ban bug\n\n");

    // 1. Reproduce the bug: a non-numeric duration token clobbers the default.
    const ParseResult buggy = parseBuggy("1.2.3.4 abc");
    check(buggy.ipAddress == "1.2.3.4", "bug repro: IP address is still parsed");
    check(buggy.durationHours == 0,
          "bug repro: malformed duration overwrites 12-hour default with 0");
    check(isPermanentBan(buggy.durationHours),
          "bug repro: that 0 is treated as a permanent ban by the ban manager");

    // 2. The fix: malformed duration keeps the documented default.
    const ParseResult fixed = parseFixed("1.2.3.4 abc");
    check(fixed.ipAddress == "1.2.3.4", "fix: IP address is still parsed");
    check(fixed.durationHours == DEFAULT_DURATION_HOURS,
          "fix: malformed duration keeps the 12-hour default");
    check(!isPermanentBan(fixed.durationHours),
          "fix: the resulting ban is timed, not permanent");

    // 3. Other malformed tokens.
    check(parseFixed("1.2.3.4 twelve").durationHours == DEFAULT_DURATION_HOURS,
          "fix: word duration keeps the default");
    check(parseFixed("1.2.3.4 -5").durationHours == DEFAULT_DURATION_HOURS,
          "fix: negative duration keeps the default");
    check(parseFixed("1.2.3.4 4.5").durationHours == DEFAULT_DURATION_HOURS,
          "fix: fractional duration keeps the default");

    // 4. Valid durations are still honoured (no regression).
    check(parseFixed("1.2.3.4 24").durationHours == 24,
          "fix: valid 24-hour duration is honoured");
    check(parseFixed("1.2.3.4 1").durationHours == 1,
          "fix: valid 1-hour duration is honoured");
    check(parseFixed("1.2.3.4 0").durationHours == 0,
          "fix: explicit 0 is preserved (existing intentional behaviour)");
    check(parseFixed("1.2.3.4   7").durationHours == 7,
          "fix: multiple spaces before duration still parse");

    // 5. The documented form with no duration still uses the default.
    check(parseFixed("1.2.3.4").durationHours == DEFAULT_DURATION_HOURS,
          "fix: documented no-duration command keeps the default");

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: banip malformed-duration bug reproduced and fix verified\n");
    return 0;
}
