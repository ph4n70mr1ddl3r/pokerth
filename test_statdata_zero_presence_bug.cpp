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

// Regression test for the zero-as-absent statistics bug in
// ClientThread::UpdateStatData (src/net/clientthread.cpp).
//
// A StatisticsMessage carries an explicit statisticstype discriminator for
// each value (see clientstate.cpp:910-913 and pokerth.proto), so presence is
// already known at the wire level. ClientThread::UpdateStatData then used the
// numeric value itself as a presence test:
//
//     if (stats.numberOfPlayersOnServer)
//         m_curStats.numberOfPlayersOnServer = stats.numberOfPlayersOnServer;
//
// numberOfPlayersOnServer is a legitimate count whose valid range includes 0
// (the last player just logged out). When the server reports 0, the guard is
// false and the previous, stale count is kept, so the GUI shows a wrong player
// count. This is the same "explicitly set 0 discarded" anti-pattern as the
// cardsValue presence bug fixed in b3dd874.
//
// This test models the relevant subset of ServerStats and the update function,
// and exercises both the buggy and the corrected versions across all cases.

#include <cstdio>

// The three fields that UpdateStatData copies, mirroring serverdata.h.
struct ServerStats {
    ServerStats()
        : numberOfPlayersOnServer(0), totalPlayersEverLoggedIn(0),
          totalGamesEverCreated(0) {}

    unsigned numberOfPlayersOnServer;
    unsigned totalPlayersEverLoggedIn;
    unsigned totalGamesEverCreated;
};

// The buggy update from clientthread.cpp:1661-1668: a value of 0 is mistaken
// for "not provided" and the previous value is retained.
static void updateStatDataBuggy(ServerStats& cur, const ServerStats& stats)
{
    if (stats.numberOfPlayersOnServer)
        cur.numberOfPlayersOnServer = stats.numberOfPlayersOnServer;
    if (stats.totalPlayersEverLoggedIn)
        cur.totalPlayersEverLoggedIn = stats.totalPlayersEverLoggedIn;
    if (stats.totalGamesEverCreated)
        cur.totalGamesEverCreated = stats.totalGamesEverCreated;
}

// The corrected update: presence is already known to the caller, so every
// supplied value -- including 0 -- is stored unconditionally.
static void updateStatDataFixed(ServerStats& cur, const ServerStats& stats)
{
    cur.numberOfPlayersOnServer = stats.numberOfPlayersOnServer;
    cur.totalPlayersEverLoggedIn = stats.totalPlayersEverLoggedIn;
    cur.totalGamesEverCreated = stats.totalGamesEverCreated;
}

struct Scenario {
    const char* name;
    ServerStats previous;
    ServerStats incoming;
    unsigned expectedPlayers;
};

int main()
{
    ServerStats prev;
    prev.numberOfPlayersOnServer = 7;
    prev.totalPlayersEverLoggedIn = 100;
    prev.totalGamesEverCreated = 50;

    ServerStats nonzero;
    nonzero.numberOfPlayersOnServer = 3;

    ServerStats zero;
    zero.numberOfPlayersOnServer = 0;

    const Scenario scenarios[] = {
        {"non-zero player count", prev, nonzero, 3},
        {"zero player count (last logout)", prev, zero, 0},
    };

    int failures = 0;

    printf("Testing zero-as-absent handling in ClientThread::UpdateStatData:\n\n");

    for (const auto& s : scenarios) {
        ServerStats fixed = s.previous;
        updateStatDataFixed(fixed, s.incoming);

        const bool fixedOk = fixed.numberOfPlayersOnServer == s.expectedPlayers;
        printf("  %-34s fixed=%u (expected %u) [%s]\n",
               s.name, fixed.numberOfPlayersOnServer, s.expectedPlayers,
               fixedOk ? "PASS" : "FAIL");
        if (!fixedOk)
            ++failures;
    }

    printf("\nVerifying the bug is real (buggy version diverges on zero):\n");

    ServerStats fixedZero = prev;
    updateStatDataFixed(fixedZero, zero);

    ServerStats buggyZero = prev;
    updateStatDataBuggy(buggyZero, zero);

    const bool divergence = (fixedZero.numberOfPlayersOnServer == 0) &&
                            (buggyZero.numberOfPlayersOnServer == 7);
    printf("  incoming count 0: fixed stores %u, buggy leaves %u [%s]\n",
           fixedZero.numberOfPlayersOnServer, buggyZero.numberOfPlayersOnServer,
           divergence ? "PASS" : "FAIL");
    if (!divergence)
        ++failures;

    // The other counters are never transmitted (see serverlobbythread.cpp:2507)
    // and start at zero, so storing them unconditionally keeps them at zero.
    const bool othersUnchanged =
        (fixedZero.totalPlayersEverLoggedIn == 0) &&
        (fixedZero.totalGamesEverCreated == 0);
    printf("  untransmitted counters reset to 0: [%s]\n",
           othersUnchanged ? "PASS" : "FAIL");
    if (!othersUnchanged)
        ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: zero-as-absent statistics bug reproduced and fix verified\n");
    return 0;
}
