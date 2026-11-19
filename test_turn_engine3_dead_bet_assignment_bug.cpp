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

// Regression test: LocalPlayer::turnEngine3() used a stale `bet` value in the
// non-all-in PLAYER_ACTION_BET branch, so the AI wagered the wrong number of
// chips (on a fresh street, zero) and announced a bet while actually checking.
//
// Background
// ----------
// src/engine/local_engine/localplayer.cpp, LocalPlayer::turnEngine3():
//
//   line 4401:  int oldMySet = mySet;
//   line 4402:  int bet = mySet;              <-- captured at function entry
//   ...
//   line 4500:  mySet = <computed bet amount>  <-- AI decides the bet here
//   ...
//   line 4519:  myCash -= bet;                <-- uses the STALE value
//   line 4520:  mySet = oldMySet + bet;       <-- uses the STALE value
//   line 4524:  bet = mySet;                  <-- dead: after the use sites
//
// On a fresh street getHighestSet() == 0 and every player's mySet was moved to
// the pot by collectPot(), so `bet` is 0. The AI computes e.g. a 30-chip bet
// into mySet, but the debit/contribution use the stale 0: myCash is unchanged,
// mySet stays 0, and setHighestSet(0) is recorded. The `bet = mySet` on line
// 4524 is too late and is never read again.
//
// The correct siblings flopEngine3() (line 4320) and riverEngine3() (line 4710)
// declare `int bet = mySet;` locally *inside* the non-all-in branch, right
// after mySet holds the computed amount. turnEngine3() is the odd one out.
//
// The fix
// -------
// Remove the function-scope `bet`, declare it fresh inside the branch, and
// drop the dead late assignment, mirroring flopEngine3()/riverEngine3().
//
// This standalone test models both semantics on the player chip state and
// asserts chip conservation: in the non-all-in BET path the amount added to
// mySet and the amount removed from myCash must both equal the decided bet.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_turn_engine3_dead_bet_assignment_bug test_turn_engine3_dead_bet_assignment_bug.cpp
//   /tmp/test_turn_engine3_dead_bet_assignment_bug

#include <cstdio>

struct ChipState {
    int mySet;
    int myCash;
    int oldMySet;
};

enum class Action { Bet, AllIn };

// Current (buggy) turnEngine3 semantics: `bet` is captured at function entry,
// and the "refresh" happens after the debit, so it never affects the result.
static Action turnBetBuggy(ChipState &s, int desiredBet)
{
    const int bet = s.mySet; // line 4401: stale, 0 on a fresh street
    s.mySet = desiredBet;    // lines 4500-4505: AI computes the bet

    if (desiredBet >= s.myCash) { // all-in branch is unaffected
        s.mySet = s.myCash;
        s.myCash = 0;
        return Action::AllIn;
    }

    s.myCash -= bet;         // line 4519: debits the stale bet
    s.mySet = s.oldMySet + bet; // line 4520: contributes the stale bet
    const int unused = s.mySet;
    (void)unused;            // line 4524: bet = mySet (dead store)
    return Action::Bet;
}

// Fixed semantics: `bet` is captured fresh after the AI computed mySet,
// exactly like flopEngine3()/riverEngine3().
static Action turnBetFixed(ChipState &s, int desiredBet)
{
    s.mySet = desiredBet; // lines 4500-4505: AI computes the bet

    if (desiredBet >= s.myCash) {
        s.mySet = s.myCash;
        s.myCash = 0;
        return Action::AllIn;
    }

    const int bet = s.mySet; // FIX: fresh value, inside the branch
    s.myCash -= bet;
    s.mySet = s.oldMySet + bet;
    return Action::Bet;
}

static int failures = 0;

static void check(const char *label, bool ok)
{
    std::printf("  %-58s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

// A non-all-in bet conserves chips iff the contribution and the cash debit
// both equal the amount the AI decided to wager.
static bool conservesChips(const ChipState &before, const ChipState &after, int desiredBet)
{
    return (after.mySet - before.mySet == desiredBet) &&
           (before.myCash - after.myCash == desiredBet);
}

static void runCase(const char *name, int entrySet, int cash, int oldSet, int desiredBet)
{
    std::printf("\n%s: entrySet=%d, cash=%d, oldSet=%d, desiredBet=%d\n",
                name, entrySet, cash, oldSet, desiredBet);

    ChipState buggy{entrySet, cash, oldSet};
    turnBetBuggy(buggy, desiredBet);
    std::printf("  buggy  -> mySet=%d, myCash=%d\n", buggy.mySet, buggy.myCash);

    ChipState fixed{entrySet, cash, oldSet};
    const Action fixedAction = turnBetFixed(fixed, desiredBet);
    std::printf("  fixed  -> mySet=%d, myCash=%d\n", fixed.mySet, fixed.myCash);

    const ChipState before{entrySet, cash, oldSet};

    if (desiredBet < cash) {
        // The bug is reproducible: stale `bet` breaks chip conservation.
        check("buggy model breaks chip conservation (bug reproduced)",
              !conservesChips(before, buggy, desiredBet));
        // The fix must conserve chips.
        check("fixed model conserves chips", conservesChips(before, fixed, desiredBet));
        check("fixed action is PLAYER_ACTION_BET", fixedAction == Action::Bet);
    } else {
        // All-in path: unchanged by the fix, both conserve all remaining cash.
        check("all-in: buggy and fixed agree on mySet", buggy.mySet == fixed.mySet);
        check("all-in: buggy and fixed agree on myCash", buggy.myCash == fixed.myCash);
        check("all-in: cash fully committed", fixed.myCash == 0 && fixed.mySet == cash);
    }
}

int main()
{
    std::printf("Testing turnEngine3() stale/dead `bet` bug (AI bet wagered 0 chips):\n");

    // Fresh street (the common case): nobody has bet, mySet was reset to 0.
    runCase("fresh street", 0, 1000, 0, 30);

    // AI still has chips committed from this street (a raise-sized bet).
    runCase("previous set present", 20, 1000, 20, 50);

    // Stale entry value larger than the intended bet (over-bet case).
    // oldMySet always equals mySet at function entry.
    runCase("stale value > desired bet", 50, 200, 50, 20);

    // All-in must behave identically before and after the fix.
    runCase("all-in", 0, 100, 0, 150);

    if (failures != 0) {
        std::printf("\n%d CHECK(S) FAILED\n", failures);
        return 1;
    }
    std::printf("\nALL CHECKS PASSED\n");
    return 0;
}
