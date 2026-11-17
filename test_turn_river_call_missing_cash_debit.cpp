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

// Regression test: missing cash debit on the AI non-all-in CALL path in
// LocalPlayer::turnEngine3() and LocalPlayer::riverEngine3()
// (src/engine/local_engine/localplayer.cpp).
//
// Background
// ----------
// In the EngineVersion=1 ("*3") AI engines, mySet is the *absolute* amount
// committed to the pot during the current betting round, and myCash is the
// stack still held back. When a player calls, both must move together:
//
//     mySet  = highestSet                (chips go into the pot)
//     myCash = myCash - highestSet + mySet   (chips leave the stack)
//
// i.e. only the difference (highestSet - mySet) is taken from the stack, since
// any chips already committed this round (mySet) must not be charged twice.
// This is exactly what the sibling RAISE branch computes with raise == 0:
//     myCash = myCash + mySet - highestSet - raise;
//
// LocalBoard::collectSets() sums every player's mySet into the pot, while
// LocalBoard::distributePot() derives each player's contribution from
// (getMyRoundStartCash() - getMyCash()) (localboard.cpp:95). Those two numbers
// must agree, otherwise chips are created from nothing: the pot holds chips no
// stack ever gave up, and distributePot() hits its "pot > 0" fallback
// (localboard.cpp:289) which dumps the orphaned chips on an arbitrary winner.
//
// The bug
// -------
// The non-all-in "CALL" else branch of turnEngine3() (line ~4592) and
// riverEngine3() (line ~4771) currently sets only mySet and myAction:
//
//     else {
//         mySet = currentHand->getCurrentBeRo()->getHighestSet();
//         myAction = PLAYER_ACTION_CALL;    // myCash NOT debited
//     }
//
// Commit d83b568 removed `myCash = myCash - highestSet + mySet;` from these
// two branches, claiming a `switch(myAction) PLAYER_ACTION_CALL` case already
// debits the cash. Such a switch exists ONLY in flopEngine3() (localplayer.cpp:
// 4370); neither turnEngine3() nor riverEngine3() has one (they do not even
// call evaluation()). The removal therefore left the CALL path with no debit
// at all -- a regression that injects (highestSet - mySet) phantom chips
// into the pot on every AI turn/river call. This is the same false-premise
// mistake that was corrected for the BET branch by commit 254bee7.
//
// This test models the ACTUAL structure of these two functions (no
// post-decision switch), the buggy and corrected CALL branches, and the
// downstream pot accounting. It asserts the corrected branch conserves chips
// and that the current branch diverges (reproducing the bug). The all-in and
// no-chips-to-call edge cases are included to show the fix is inert there.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_turn_river_call_missing_cash_debit test_turn_river_call_missing_cash_debit.cpp
//   /tmp/test_turn_river_call_missing_cash_debit

#include <cstdio>

enum PlayerAction {
    ACTION_NONE,
    ACTION_CHECK,
    ACTION_CALL,
    ACTION_ALLIN,
};

struct Player {
    int roundStartCash;  // cash at the start of the hand
    int cash;            // myCash
    int set;             // mySet (absolute, this betting round)
    PlayerAction action;
};

// Faithful model of the end of turnEngine3()/riverEngine3(): there is NO
// switch(myAction) post-processing in these functions, so the CALL else branch
// is the only opportunity to debit the stack.
static void callCurrent(Player& p, int highestSet)
{
    if (highestSet >= p.cash + p.set) {   // all-in call
        p.set += p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.set = highestSet;                // BUG: myCash is never reduced here
        p.action = ACTION_CALL;
    }
    // no switch(myAction), no evaluation() -> nothing else touches myCash
    if (p.cash < 0) p.cash = 0;            // myCash = std::max(0, myCash)
}

static void callFixed(Player& p, int highestSet)
{
    if (highestSet >= p.cash + p.set) {
        p.set += p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.cash = p.cash - highestSet + p.set;   // FIX: chips leave the stack once
        p.set = highestSet;
        p.action = ACTION_CALL;
    }
    if (p.cash < 0) p.cash = 0;
}

// Model the pot accounting performed by collectSets() + distributePot().
struct Accounting {
    int chipsInPot;           // sum of mySet that collectSets() moved to the pot
    int computedContribution; // distributePot(): roundStartCash - myCash
    int orphanedChips;        // chips in pot with no contributing stack
};

static Accounting account(const Player& p, int opponentBet)
{
    Accounting a;
    a.chipsInPot = opponentBet + p.set;
    a.computedContribution = (p.roundStartCash - p.cash) + opponentBet;
    a.orphanedChips = a.chipsInPot - a.computedContribution;
    return a;
}

static bool runScenario(const char* label, int startCash, int priorSet, int highestSet, bool allIn)
{
    printf("  %s (startCash=%d, priorSet=%d, highestSet=%d, allIn=%s)\n",
           label, startCash, priorSet, highestSet, allIn ? "yes" : "no");

    bool ok = true;
    const int opponentBet = highestSet;

    // --- corrected branch: must conserve chips ---
    Player fixed{startCash, startCash - priorSet, priorSet, ACTION_NONE};
    callFixed(fixed, highestSet);
    Accounting af = account(fixed, opponentBet);
    bool fixedConserves = (af.orphanedChips == 0);
    bool fixedStackOk = fixed.action == ACTION_ALLIN
                            ? (fixed.cash == 0)
                            : (fixed.cash == startCash - highestSet && fixed.set == highestSet);
    printf("    fixed:   cash=%d set=%d action=%s | pot=%d contribution=%d orphaned=%d [%s]\n",
           fixed.cash, fixed.set,
           fixed.action == ACTION_ALLIN ? "ALLIN" : "CALL",
           af.chipsInPot, af.computedContribution, af.orphanedChips,
           (fixedConserves && fixedStackOk) ? "PASS" : "FAIL");
    if (!fixedConserves || !fixedStackOk) ok = false;

    // --- current branch: all-in and zero-to-call are fine; a real call injects chips ---
    Player current{startCash, startCash - priorSet, priorSet, ACTION_NONE};
    callCurrent(current, highestSet);
    Accounting ac = account(current, opponentBet);
    const int toCall = highestSet - priorSet;
    if (allIn || toCall == 0) {
        bool unaffected = (ac.orphanedChips == 0);
        printf("    current: cash=%d set=%d action=%s | orphaned=%d [%s]\n",
               current.cash, current.set,
               current.action == ACTION_ALLIN ? "ALLIN" : "CALL",
               ac.orphanedChips, unaffected ? "PASS (edge case unaffected)" : "FAIL");
        if (!unaffected) ok = false;
    } else {
        bool bugReproduced = (ac.orphanedChips == toCall) &&
                             (current.cash == startCash - priorSet) &&
                             (current.set == highestSet);
        printf("    current: cash=%d set=%d action=CALL | pot=%d contribution=%d orphaned=%d [%s]\n",
               current.cash, current.set, ac.chipsInPot, ac.computedContribution,
               ac.orphanedChips, bugReproduced ? "bug reproduced" : "NO DIVERGENCE");
        if (!bugReproduced) ok = false;
    }

    return ok;
}

int main()
{
    int failures = 0;

    printf("Testing missing myCash debit on AI turn/river CALL (chip conservation):\n\n");

    // A normal turn call: face a 100 bet with a full 1000 stack.
    if (!runScenario("turn call", 1000, 0, 100, false)) ++failures;
    // The same code path is reached on the river.
    if (!runScenario("river call", 1000, 0, 250, false)) ++failures;
    // Call after already committing chips this round (e.g. posted blind);
    // only the difference must leave the stack.
    if (!runScenario("call with prior set", 1000, 30, 100, false)) ++failures;
    // A large pot-relative call.
    if (!runScenario("large call", 500, 0, 200, false)) ++failures;
    // No chips left to call: must be a no-op on the stack.
    if (!runScenario("zero to call", 1000, 100, 100, false)) ++failures;
    // The all-in call branch must be unchanged by the fix.
    if (!runScenario("all-in call", 100, 0, 1000, true)) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: missing CALL cash debit reproduced and fix verified\n");
    return 0;
}
