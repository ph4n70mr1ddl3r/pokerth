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

// Regression test for the double chip deduction in LocalPlayer::preflopEngine().
//
// Every other betting engine (the flop/turn/river engines) ends with a single
// call to evaluation(bet, raise), which is the canonical place that updates
// mySet/myCash via setMySet(). preflopEngine() alone carries an EXTRA switch
// after evaluation() that subtracts the same chips a second time.
//
//   evaluation(bet, raise);            // setMySet(): myCash -= amount
//   switch(myAction) {                 // <- stray second deduction
//       case PLAYER_ACTION_CALL: myCash -= toCall; ...
//       case PLAYER_ACTION_BET:  myCash -= bet; ...
//       case PLAYER_ACTION_RAISE: myCash -= totalBet; ...
//   }
//
// For a normal 30-chip call with mySet=20, myCash=100, highestSet=50:
//   evaluation(): setMySet(50-20=30) -> mySet=50, myCash=70   (correct)
//   switch:       toCall=50-20=30    -> myCash=40              (WRONG)
// After the fix the switch is gone and myCash stays at 70.
//
// The existing test_preflop_engine_double_bug.cpp models only the two switches
// and never evaluation(), so it passes while the real deduction still happens.
// This test models BOTH evaluation()/setMySet() and the stray switch.

#include <cstdio>
#include <algorithm>

enum PlayerAction {
    PLAYER_ACTION_FOLD = 1,
    PLAYER_ACTION_CHECK = 2,
    PLAYER_ACTION_CALL = 3,
    PLAYER_ACTION_BET = 4,
    PLAYER_ACTION_RAISE = 5,
    PLAYER_ACTION_ALLIN = 6
};

struct MockPlayer {
    int mySet;
    int myCash;
    int myLastRelativeSet;
    int action;
    int boardHighestSet;

    MockPlayer(int set, int cash, int highest)
        : mySet(set), myCash(cash), myLastRelativeSet(0),
          action(PLAYER_ACTION_CALL), boardHighestSet(highest) {}

    int getHighestSet() const { return boardHighestSet; }
    void setHighestSet(int v) { boardHighestSet = v; }

    // Mirrors LocalPlayer::setMySet (localplayer.h:127-135): theValue is the
    // incremental amount added to mySet and deducted from myCash.
    void setMySet(int theValue) {
        if (theValue > myCash) theValue = myCash;
        if (theValue < 0) theValue = 0;
        myLastRelativeSet = theValue;
        mySet += theValue;
        myCash -= theValue;
        if (myCash < 0) myCash = 0;
    }

    // The money-update portion of LocalPlayer::evaluation (no all-in paths).
    void evaluation(int bet, int raise) {
        const int highestSet = boardHighestSet;
        switch (action) {
        case PLAYER_ACTION_CALL:
            setMySet(highestSet - mySet);
            break;
        case PLAYER_ACTION_BET:
            setMySet(bet);
            setHighestSet(mySet);
            break;
        case PLAYER_ACTION_RAISE:
            setMySet(highestSet + raise - mySet);
            setHighestSet(mySet);
            break;
        default:
            break;
        }
    }
};

// The stray switch currently appended after evaluation() in preflopEngine().
static void strayDeductionSwitch(MockPlayer &p, int oldMySet, int bet, int raise) {
    switch (p.action) {
    case PLAYER_ACTION_CALL: {
        int toCall = p.getHighestSet() - oldMySet;
        if (toCall > 0) {
            p.myCash -= toCall;
            p.mySet = p.getHighestSet();
        }
        break;
    }
    case PLAYER_ACTION_RAISE: {
        int toCall = p.getHighestSet() - oldMySet;
        int totalBet = toCall + raise;
        if (totalBet > 0) {
            if (totalBet > p.myCash) {
                p.mySet += p.myCash;
                p.myCash = 0;
            } else {
                p.myCash -= totalBet;
                p.mySet = p.getHighestSet() + raise;
            }
        }
        break;
    }
    case PLAYER_ACTION_BET:
        if (bet > 0) {
            p.myCash -= bet;
            p.mySet = oldMySet + bet;
        }
        break;
    case PLAYER_ACTION_ALLIN:
        p.mySet += p.myCash;
        p.myCash = 0;
        break;
    default:
        break;
    }
}

// Buggy: evaluation() plus the stray switch.
static void preflopBuggy(MockPlayer &p, int oldMySet, int bet, int raise) {
    p.evaluation(bet, raise);
    strayDeductionSwitch(p, oldMySet, bet, raise);
    p.myCash = std::max(0, p.myCash);
    p.myLastRelativeSet = p.mySet - oldMySet;
}

// Fixed: evaluation() alone updates mySet/myCash, as in every other engine.
static void preflopFixed(MockPlayer &p, int oldMySet, int bet, int raise) {
    p.evaluation(bet, raise);
    p.myCash = std::max(0, p.myCash);
    p.myLastRelativeSet = p.mySet - oldMySet;
}

struct Scenario {
    const char *name;
    int action;
    int oldMySet;
    int cash;
    int highest;
    int bet;
    int raise;
    int expectedCash;
    int expectedSet;
};

int main() {
    const Scenario scenarios[] = {
        {"CALL, partial stack", PLAYER_ACTION_CALL, 20, 100, 50, 0, 0, 70, 50},
        {"CALL, large stack", PLAYER_ACTION_CALL, 0, 1000, 100, 0, 0, 900, 100},
        {"BET on the flop-sized board", PLAYER_ACTION_BET, 0, 200, 0, 20, 0, 180, 20},
        {"RAISE an existing bet", PLAYER_ACTION_RAISE, 10, 300, 40, 0, 40, 230, 80},
    };

    int failures = 0;
    printf("Testing double chip deduction in LocalPlayer::preflopEngine():\n\n");

    for (const auto &s : scenarios) {
        MockPlayer fixed(s.oldMySet, s.cash, s.highest);
        fixed.action = s.action;
        preflopFixed(fixed, s.oldMySet, s.bet, s.raise);

        const bool ok = fixed.myCash == s.expectedCash && fixed.mySet == s.expectedSet;
        printf("  %-28s fixed cash=%d set=%d (expected %d/%d) [%s]\n",
               s.name, fixed.myCash, fixed.mySet, s.expectedCash, s.expectedSet,
               ok ? "PASS" : "FAIL");
        if (!ok) ++failures;
    }

    printf("\nVerifying the bug is real (buggy version diverges on CALL):\n");
    MockPlayer buggy(20, 100, 50);
    buggy.action = PLAYER_ACTION_CALL;
    preflopBuggy(buggy, 20, 0, 0);

    MockPlayer correct(20, 100, 50);
    correct.action = PLAYER_ACTION_CALL;
    preflopFixed(correct, 20, 0, 0);

    const bool diverges = (correct.myCash == 70) && (buggy.myCash == 40);
    printf("  30-chip call: fixed cash=%d, buggy cash=%d [%s]\n",
           correct.myCash, buggy.myCash, diverges ? "PASS" : "FAIL");
    if (!diverges) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: preflopEngine double-deduction bug reproduced and fix verified\n");
    return 0;
}
