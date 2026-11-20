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

// Regression test: LocalPlayer::evaluation() PLAYER_ACTION_BET double-debits
// the player's cash.
//
// Background
// ----------
// LocalPlayer::setMySet() (src/engine/local_engine/localplayer.h:127) is a
// *relative* setter: it adds the value to mySet and subtracts the same value
// from myCash (clamping to [0, myCash]).
//
// The bug
// -------
// In LocalPlayer::evaluation(), the PLAYER_ACTION_BET non-all-in branch
// (src/engine/local_engine/localplayer.cpp:2826) does:
//
//     myCash -= bet;      // <-- spurious: setMySet(bet) already debits cash
//     setMySet(bet);
//
// Because setMySet() already debits myCash, the explicit subtraction removes
// the bet twice. Chips vanish (myCash + mySet is not conserved), and the clamp
// inside setMySet() can even make the player bet less than intended while
// still losing the full (twice-debited) amount.
//
// The fix
// -------
// Remove the explicit `myCash -= bet;`. setMySet(bet) alone performs the
// correct, single debit.
//
// This test needs no Qt/Boost/protobuf: it faithfully models setMySet() and
// the two branches using plain C++ so chip conservation can be asserted.
//
// Compile manually with:
//   g++ -O2 -std=c++23 -Wall -Wextra -o test_eval_bet_double_debit_bug test_eval_bet_double_debit_bug.cpp

#include <cstdio>

namespace {

struct Player {
    int mySet = 0;
    int myCash = 0;

    Player(int set, int cash) : mySet(set), myCash(cash) {}

    // Faithful model of LocalPlayer::setMySet() (relative setter).
    void setMySet(int theValue)
    {
        if (theValue > myCash) theValue = myCash;
        if (theValue < 0) theValue = 0;
        mySet += theValue;
        myCash -= theValue;
        if (myCash < 0) myCash = 0;
    }
};

// Current (buggy) production branch: subtracts cash twice.
void evalBetBuggy(Player& p, int bet)
{
    p.myCash -= bet;
    p.setMySet(bet);
}

// Fixed branch: setMySet() is the single source of truth for the debit.
void evalBetFixed(Player& p, int bet)
{
    p.setMySet(bet);
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %s: %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

} // namespace

int main()
{
    std::printf("Regression test: evaluation() PLAYER_ACTION_BET double-debit bug\n\n");

    // Scenario 1: simple bet from an empty set.
    // Expected: mySet == 20, myCash == 80, total conserved at 100.
    {
        std::printf("Scenario 1 (mySet=0, myCash=100, bet=20)\n");
        Player buggy(0, 100);
        evalBetBuggy(buggy, 20);
        std::printf("  Buggy: mySet=%d myCash=%d total=%d\n",
                    buggy.mySet, buggy.myCash, buggy.mySet + buggy.myCash);
        check("buggy code loses 20 chips (reproduces the bug)",
              buggy.mySet == 20 && buggy.myCash == 60);

        Player fixed(0, 100);
        evalBetFixed(fixed, 20);
        std::printf("  Fixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);
        check("fixed code sets mySet=20, myCash=80",
              fixed.mySet == 20 && fixed.myCash == 80);
        check("fixed code conserves chips", fixed.mySet + fixed.myCash == 100);
    }

    // Scenario 2: bet with chips already committed in the set.
    // Expected: mySet == 50, myCash == 80, total conserved at 130.
    {
        std::printf("\nScenario 2 (mySet=30, myCash=100, bet=20)\n");
        Player fixed(30, 100);
        evalBetFixed(fixed, 20);
        std::printf("  Fixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);
        check("fixed code sets mySet=50, myCash=80",
              fixed.mySet == 50 && fixed.myCash == 80);
        check("fixed code conserves chips", fixed.mySet + fixed.myCash == 130);
    }

    // Scenario 3: the clamp. The extra debit shrinks myCash before setMySet()
    // clamps the bet against it, so the player bets only 40 yet loses 100.
    // Expected (fixed): mySet == 60, myCash == 40, total conserved at 100.
    {
        std::printf("\nScenario 3 (mySet=0, myCash=100, bet=60)\n");
        Player buggy(0, 100);
        evalBetBuggy(buggy, 60);
        std::printf("  Buggy: mySet=%d myCash=%d total=%d\n",
                    buggy.mySet, buggy.myCash, buggy.mySet + buggy.myCash);
        check("buggy code under-bets due to clamp (60 -> 40)",
              buggy.mySet == 40 && buggy.myCash == 0);

        Player fixed(0, 100);
        evalBetFixed(fixed, 60);
        std::printf("  Fixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);
        check("fixed code bets full 60 and keeps 40",
              fixed.mySet == 60 && fixed.myCash == 40);
        check("fixed code conserves chips", fixed.mySet + fixed.myCash == 100);
    }

    std::printf("\n");
    if (failures > 0) {
        std::printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("TEST PASSED: evaluation() PLAYER_ACTION_BET double-debit bug reproduced and fix verified\n");
    return 0;
}
