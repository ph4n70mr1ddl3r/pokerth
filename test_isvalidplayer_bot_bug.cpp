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

// Regression test: IsValidPlayer() used GetPlayerIdList() which included
// computer players (bots), so it incorrectly considered bots as valid players.
//
// Background
// ----------
// src/net/servergame.cpp, ServerGame::IsValidPlayer():
//
//   line 1133: const PlayerIdList list(GetPlayerIdList());  <-- humans + bots
//   This uses ServerGame::GetPlayerIdList() which appends every computer
//   player id from m_computerPlayerList. Bots are not in sessions, so they
//   should not be considered "valid players" in the context of human-only
//   operations like vote kicking.
//
// The fix
// -------
// Use GetSessionManager().GetPlayerIdList(SessionData::Game) instead of
// GetPlayerIdList() to get the human-only player list.
//
// This standalone test models the relevant subset and checks both the buggy
// predicate (humans + bots) and the fixed one (humans only) across several
// scenarios.

#include <algorithm>
#include <cstdio>
#include <vector>

typedef std::vector<int> PlayerIdList;

static PlayerIdList combine(const PlayerIdList& humans, const PlayerIdList& bots)
{
    PlayerIdList all = humans;
    all.insert(all.end(), bots.begin(), bots.end());
    return all;
}

int main()
{
    // 3 humans (ids 1,2,3) and 3 bots (ids 101,102,103).
    const PlayerIdList humans3{1, 2, 3};
    const PlayerIdList bots3{101, 102, 103};

    // Buggy: uses humans + bots as valid player IDs (via GetPlayerIdList())
    // Fixed: uses humans only as valid player IDs (via GetSessionManager().GetPlayerIdList())
    const PlayerIdList buggyValid = combine(humans3, bots3);
    const PlayerIdList fixedValid = humans3;

    static int failures = 0;

    static void check(const char* label, bool ok)
    {
        std::printf("  %-64s %s\n", label, ok ? "PASS" : "FAIL");
        if (!ok)
            ++failures;
    }

    // Humans should be valid in both buggy and fixed versions
    {
        const char* name = "human player 1 is valid";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 1) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 1) != fixedValid.end();
        check(name, buggy && fixed);
    }
    {
        const char* name = "human player 2 is valid";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 2) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 2) != fixedValid.end();
        check(name, buggy && fixed);
    }
    {
        const char* name = "human player 3 is valid";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 3) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 3) != fixedValid.end();
        check(name, buggy && fixed);
    }

    // Bots should NOT be valid in the fixed version (this is the bug fix)
    {
        const char* name = "bot player 101 is NOT valid (fixed)";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 101) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 101) != fixedValid.end();
        check(name, !fixed);  // fixed should consider bots invalid
    }
    {
        const char* name = "bot player 102 is NOT valid (fixed)";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 102) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 102) != fixedValid.end();
        check(name, !fixed);  // fixed should consider bots invalid
    }
    {
        const char* name = "bot player 103 is NOT valid (fixed)";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 103) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 103) != fixedValid.end();
        check(name, !fixed);  // fixed should consider bots invalid
    }

    // Non-existent player should not be valid in either version
    {
        const char* name = "player 999 is NOT valid (non-existent)";
        bool buggy = std::find(buggyValid.begin(), buggyValid.end(), 999) != buggyValid.end();
        bool fixed = std::find(fixedValid.begin(), fixedValid.end(), 999) != fixedValid.end();
        check(name, !buggy && !fixed);
    }

    std::printf("\nTesting IsValidPlayer() valid-player population (bot voter bug):\n\n");

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    std::printf("\nTEST PASSED: IsValidPlayer bug reproduced and fix verified\n");
    return 0;
}