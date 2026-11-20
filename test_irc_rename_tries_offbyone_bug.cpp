/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2013 Felix Hammer, Florian Thauer, Lothar May          *
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

// Regression test: irc_auto_rename_nick() performs one rename too many.
//
// Background
// ----------
// irc_auto_rename_nick() (src/net/ircthread.cpp:108) handles IRC
// ERR_NICKNAMEINUSE / ERR_NICKCOLLISION. It is governed by:
//
//     #define IRC_MAX_RENAME_TRIES  5          (ircthread.cpp:63)
//     if (context->renameTries <= IRC_MAX_RENAME_TRIES) { ...rename... }
//
// renameTries starts at 0 (IrcContext constructor, ircthread.cpp:92) and is
// incremented once per rename (ircthread.cpp:137). With the "<=" guard the
// function renames for counter values 0,1,2,3,4,5 -- six attempts -- and only
// quits on the seventh collision. That contradicts the constant's name and its
// inline comment "Limit number of rename tries".
//
// The bug
// -------
// A MAX_* limit counted from zero must use "<": "<= MAX" allows MAX+1 tries.
// An IRC server that keeps rejecting the bot's nick is spammed with six NICK
// commands instead of the intended five before the bot gives up.
//
// The fix
// -------
//     if (context->renameTries < IRC_MAX_RENAME_TRIES) {
//
// This standalone test models the exact counter state machine (no Qt, Boost or
// libircclient needed), drives it through repeated collisions, and checks that
// the buggy "<= " model issues six renames while the fixed "<" model issues
// exactly five and then quits.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o test_irc_rename_tries_offbyone_bug test_irc_rename_tries_offbyone_bug.cpp

#include <cstdio>

namespace {

constexpr int kMaxRenameTries = 5;

struct Outcome {
    int renames = 0;
    int quits = 0;
};

// Model of irc_auto_rename_nick() driven by repeated nickname collisions.
// useLT == false models the buggy "<= IRC_MAX_RENAME_TRIES";
// useLT == true  models the fixed  "< IRC_MAX_RENAME_TRIES".
Outcome simulate(unsigned collisions, bool useLT)
{
    Outcome result;
    unsigned renameTries = 0;
    for (unsigned collision = 0; collision < collisions; ++collision) {
        const bool allowed = useLT ? (renameTries < kMaxRenameTries)
                                   : (renameTries <= kMaxRenameTries);
        if (allowed) {
            ++result.renames;
            ++renameTries;
        } else {
            ++result.quits;
        }
    }
    return result;
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-74s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    std::printf("Regression test: IRC auto-rename off-by-one (IRC_MAX_RENAME_TRIES=%d)\n\n",
                kMaxRenameTries);

    // Eight collisions: enough to exhaust either limit and observe the quit.
    const unsigned collisions = 8;

    const Outcome buggy = simulate(collisions, false);
    const Outcome fixed = simulate(collisions, true);

    std::printf("1) Buggy '<= MAX' model:\n");
    check("issues 6 renames (MAX+1) before quitting", buggy.renames == kMaxRenameTries + 1);
    check("quits only on the 7th collision", buggy.quits == collisions - (kMaxRenameTries + 1));

    std::printf("\n2) Fixed '< MAX' model:\n");
    check("issues exactly 5 renames", fixed.renames == kMaxRenameTries);
    check("quits on the 6th collision", fixed.quits == collisions - kMaxRenameTries);

    std::printf("\n3) The fix removes exactly one rename attempt:\n");
    check("buggy - fixed == 1", buggy.renames - fixed.renames == 1);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: off-by-one reproduced and '< MAX' fix verified\n");
    return 0;
}
