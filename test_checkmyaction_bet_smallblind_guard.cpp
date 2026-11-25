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
 ****************************************************************************/

// Regression test: checkMyAction() PLAYER_ACTION_BET smallBlind > 0 guard
//
// Bug: In checkMyAction(), the PLAYER_ACTION_BET case had condition
// (smallBlind == 0 || targetBet >= 2*smallBlind) which accepted any bet
// when smallBlind was 0, even though a 0 small blind is invalid.
//
// Fix: Changed condition to (smallBlind > 0 && targetBet >= 2*smallBlind)
// which correctly rejects bets when smallBlind is 0.
//
// This test models both the buggy and fixed semantics and verifies correct
// action validation.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state matching the actual code structure
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
};

// Buggy checkMyAction PLAYER_ACTION_BET: old condition (smallBlind == 0 || targetBet >= 2*smallBlind)
static int checkMyActionBuggy(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind) {
    switch(targetAction) {
    case 3: // PLAYER_ACTION_BET
        if(highestSet == 0 && targetBet <= 100 && (smallBlind == 0 || targetBet >= 2*smallBlind)) {
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

// Fixed checkMyAction PLAYER_ACTION_BET: new condition (smallBlind > 0 && targetBet >= 2*smallBlind)
static int checkMyActionFixed(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind) {
    switch(targetAction) {
    case 3: // PLAYER_ACTION_BET
        if(highestSet == 0 && targetBet <= 100 && (smallBlind > 0 && targetBet >= 2*smallBlind)) {
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

int failures = 0;

static void check(const char *label, bool ok)
{
    std::printf("  %-60s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

static void runCase(const char *name, int targetBet, int smallBlind, int expectedBuggy, int expectedFixed)
{
    std::printf("\n%s (bet=%d, smallBlind=%d):\n", name, targetBet, smallBlind);

    MockLocalPlayer p;

    // Test buggy
    int resultBuggy = checkMyActionBuggy(3, targetBet, 0, 0, smallBlind);
    std::printf("  buggy: result=%d (expected %d)\n", resultBuggy, expectedBuggy);
    check("buggy correct", resultBuggy == expectedBuggy);

    // Test fixed
    int resultFixed = checkMyActionFixed(3, targetBet, 0, 0, smallBlind);
    std::printf("  fixed: result=%d (expected %d)\n", resultFixed, expectedFixed);
    check("fixed correct", resultFixed == expectedFixed);
}

int main()
{
    std::printf("Testing checkMyAction() PLAYER_ACTION_BET smallBlind > 0 guard:\n");

    // Scenario 1: smallBlind=0 should reject any bet (the key bug fix)
    runCase("smallBlind=0 should reject", 20, 0, 1, 1);

    // Scenario 2: smallBlind=10, bet=20 (valid: 20 >= 2*10) - both should accept
    runCase("smallBlind=10, bet=20 valid", 20, 10, 0, 0);

    // Scenario 3: smallBlind=10, bet=5 (invalid: 5 < 2*10) - both should reject
    runCase("smallBlind=10, bet=5 invalid", 5, 10, 1, 1);

    // Scenario 4: smallBlind=10, bet=0 (invalid: bet < 2*smallBlind) - both should reject
    runCase("smallBlind=10, bet=0 invalid", 0, 10, 1, 1);

    // Scenario 5: smallBlind=5, bet=10 (valid: 10 >= 2*5) - both should accept
    runCase("smallBlind=5, bet=10 valid", 10, 5, 0, 0);

    std::printf("\n");
    if (failures > 0) {
        std::printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}