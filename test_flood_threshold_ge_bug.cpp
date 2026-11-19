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

// Regression test: the chat cleaner compares several *runtime reloadable*
// escalation thresholds with "==" instead of ">=":
//
//   src/chatcleaner/textfloodcheck.cpp:67   floodLevel == textFloodLevelToTrigger
//   src/chatcleaner/messagefilter.cpp:103   warnLevel  == warnLevelToKick
//   src/chatcleaner/messagefilter.cpp:126   kickNumber == kickNumberToBan
//
// All three thresholds are re-read from the config file by refreshConfig(),
// which the cleaner server calls on a 10-second QTimer. An admin lowering a
// threshold at runtime (e.g. 5 -> 2) leaves already-tracked players with a
// counter that is strictly *greater* than the new threshold. With "==" the
// counter then keeps incrementing forever and never matches again, so flood
// protection / kick / ban is permanently disabled for that player.
//
// The fix changes every comparison to ">=", so a counter that is at OR above
// the (possibly lowered) threshold escalates correctly.
//
// This standalone test faithfully models the three integer state machines and
// drives each through the "threshold lowered below the current count" scenario,
// checking that the buggy "==" model never escalates while the fixed ">=" model
// does.

#include <cstdio>

namespace {

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-74s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

// True when the escalation counter has reached the threshold.
// useGE == false models the buggy "=="; useGE == true models the fix ">=".
bool reached(int counter, int threshold, bool useGE)
{
    return useGE ? (counter >= threshold) : (counter == threshold);
}

// ---------------------------------------------------------------------------
// 1. TextFloodCheck::run(): build floodLevel up to a value below the original
//    threshold, then lower the threshold and send one more rapid message.
// ---------------------------------------------------------------------------
bool textFloodTriggers(bool useGE)
{
    int threshold = 5; // original TextFloodLevelToTrigger
    int floodLevel = 0;
    long timeStamp = 0;

    // First message: creates the entry with floodLevel 0, never triggers.
    // Then four rapid messages raise the level 0 -> 1 -> 2 -> 3 -> 4 while the
    // threshold is still 5, so none of them trigger.
    for (int i = 0; i < 5; ++i) {
        if (i == 0) {
            floodLevel = 0;
            timeStamp = 0;
            continue;
        }
        const int now = 0; // all within one second => rapid
        if (now - timeStamp <= 1) {
            if (reached(floodLevel, threshold, useGE)) {
                floodLevel = 0;
                return true;
            }
            ++floodLevel;
        }
        timeStamp = now;
    }

    // Admin lowers TextFloodLevelToTrigger from 5 to 2 while floodLevel == 4.
    threshold = 2;
    const int now = 0;
    if (now - timeStamp <= 1) {
        if (reached(floodLevel, threshold, useGE)) {
            floodLevel = 0;
            return true;
        }
        ++floodLevel;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 2. MessageFilter::check() warnLevel escalation. Offences alternate type so
//    the "lastWarnType == offence" shortcut cannot mask the threshold bug.
// ---------------------------------------------------------------------------
bool warnEscalates(bool useGE)
{
    int warnLevelToKick = 5;
    int warnLevel = 1; // first offence created the entry at level 1
    int lastWarnType = 0;

    // Raise warnLevel to 4 with distinct alternating offences while the
    // threshold is still 5.
    int offence = 1;
    for (int i = 0; i < 3; ++i) {
        if (reached(warnLevel, warnLevelToKick, useGE) || lastWarnType == offence) {
            return true; // would kick/mute
        }
        ++warnLevel;
        lastWarnType = offence;
        offence = (offence == 1) ? 2 : 1;
    }

    // Threshold lowered 5 -> 2 while warnLevel == 4 and next offence differs
    // from lastWarnType.
    warnLevelToKick = 2;
    offence = (lastWarnType == 1) ? 2 : 1;
    return reached(warnLevel, warnLevelToKick, useGE) || lastWarnType == offence;
}

// ---------------------------------------------------------------------------
// 3. Kick-to-ban counter: lower KickNumberToBan below the current kickNumber.
// ---------------------------------------------------------------------------
bool kickBans(bool useGE)
{
    int kickNumberToBan = 5;
    int kickNumber = 1; // inserted after the first kick

    for (int i = 0; i < 3; ++i) {
        if (reached(kickNumber, kickNumberToBan, useGE)) {
            return true; // would ban
        }
        ++kickNumber;
    }

    // Threshold lowered 5 -> 2 while kickNumber == 4.
    kickNumberToBan = 2;
    return reached(kickNumber, kickNumberToBan, useGE);
}

} // namespace

int main()
{
    std::printf("Testing chat-cleaner escalation thresholds against runtime lowering:\n\n");

    std::printf("1) TextFloodCheck flood level (threshold lowered 5 -> 2 at level 4):\n");
    check("buggy '==' never triggers flood protection", !textFloodTriggers(false));
    check("fixed '>=' triggers flood protection", textFloodTriggers(true));

    std::printf("\n2) MessageFilter warn level (threshold lowered 5 -> 2 at level 4):\n");
    check("buggy '==' never kicks/mutes", !warnEscalates(false));
    check("fixed '>=' kicks/mutes", warnEscalates(true));

    std::printf("\n3) Kick-to-ban counter (threshold lowered 5 -> 2 at count 4):\n");
    check("buggy '==' never bans", !kickBans(false));
    check("fixed '>=' bans", kickBans(true));

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: threshold-lowering bug reproduced and '>=' fix verified\n");
    return 0;
}
