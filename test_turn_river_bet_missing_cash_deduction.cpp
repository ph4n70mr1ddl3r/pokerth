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

// Regression test: missing cash debit on the AI non-all-in BET path in
// LocalPlayer::turnEngine3() and LocalPlayer::riverEngine3()
// (src/engine/local_engine/localplayer.cpp).
//
// Background
// ----------
// In the EngineVersion=1 ("*3") AI engines, mySet is the *absolute* amount
// committed to the pot during the current betting round, and myCash is the
// stack still held back. When a player bets, both must move together:
//
//     mySet  += bet        (chips go into the pot)
//     myCash -= bet        (chips leave the stack)
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
// The non-all-in "BET" else branch of turnEngine3() (line ~4542) and
// riverEngine3() (line ~4723) currently sets only myAction and mySet:
//
//     else {
//         myAction = PLAYER_ACTION_BET;    // myCash NOT debited
//     }
//
// Commit 56866f9 removed `myCash -= mySet;` from these two branches, claiming
// a `switch(myAction) PLAYER_ACTION_BET` case already debits the cash. Such a
// switch exists ONLY in flopEngine3() (localplayer.cpp:4370); neither
// turnEngine3() nor riverEngine3() has one (they do not even call evaluation()).
// The removal therefore left the BET path with no debit at all -- a regression
// that injects chips into the pot on every AI turn/river bet.
//
// This test models the ACTUAL structure of these two functions (no post-decision
// switch), the buggy and corrected BET branches, and the downstream pot
// accounting. It asserts the corrected branch conserves chips and that the
// current branch diverges (reproducing the bug). The all-in branch is included
// to show the fix does not affect it.
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_turn_river_bet_missing_cash_deduction \
//       test_turn_river_bet_missing_cash_deduction.cpp && \
//   /tmp/test_turn_river_bet_missing_cash_deduction

#include <cstdio>

enum PlayerAction {
    ACTION_NONE,
    ACTION_CHECK,
    ACTION_BET,
    ACTION_ALLIN,
};

struct Player {
    int roundStartCash;
    int cash;  // myCash
    int set;   // mySet (absolute, this betting round)
    PlayerAction action;
};

// Faithful model of the end of turnEngine3()/riverEngine3(): there is NO
// switch(myAction) post-processing in these functions, so the BET else branch
// is the only opportunity to debit the stack.
static void betCurrent(Player& p, int bet)
{
    p.set = bet;                        // mySet = <computed bet amount>
    if (bet >= p.cash) {                // mySet >= myCash -> all-in
        p.set = p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.action = ACTION_BET;          // BUG: myCash is never reduced here
    }
    // no switch(myAction), no evaluation() -> nothing else touches myCash
}

static void betFixed(Player& p, int bet)
{
    p.set = bet;
    if (bet >= p.cash) {
        p.set = p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.cash -= p.set;                // FIX: chips leave the stack once
        p.action = ACTION_BET;
    }
}

// Model the pot accounting performed by collectSets() + distributePot().
struct Accounting {
    int chipsInPot;           // sum of mySet that collectSets() moved to the pot
    int computedContribution; // distributePot(): roundStartCash - myCash
    int orphanedChips;        // chips in pot with no contributing stack
};

static Accounting account(const Player& p)
{
    Accounting a;
    a.chipsInPot = p.set;
    a.computedContribution = p.roundStartCash - p.cash;
    a.orphanedChips = a.chipsInPot - a.computedContribution;
    return a;
}

static bool runScenario(const char* label, int startCash, int bet, bool allIn)
{
    printf("  %s (startCash=%d, bet=%d, allIn=%s)\n",
           label, startCash, bet, allIn ? "yes" : "no");

    bool ok = true;

    // --- corrected branch: must conserve chips ---
    Player fixed{startCash, startCash, 0, ACTION_NONE};
    betFixed(fixed, bet);
    Accounting af = account(fixed);
    bool fixedConserves = (af.orphanedChips == 0) &&
                          (af.computedContribution == af.chipsInPot);
    bool fixedStackOk = fixed.action == ACTION_ALLIN
                            ? (fixed.cash == 0 && fixed.set == startCash)
                            : (fixed.cash == startCash - bet && fixed.set == bet);
    printf("    fixed:   cash=%d set=%d action=%s | pot=%d contribution=%d orphaned=%d [%s]\n",
           fixed.cash, fixed.set,
           fixed.action == ACTION_ALLIN ? "ALLIN" : "BET",
           af.chipsInPot, af.computedContribution, af.orphanedChips,
           (fixedConserves && fixedStackOk) ? "PASS" : "FAIL");
    if (!fixedConserves || !fixedStackOk) ok = false;

    // --- current branch: all-in is fine; non-all-in BET injects chips ---
    Player current{startCash, startCash, 0, ACTION_NONE};
    betCurrent(current, bet);
    Accounting ac = account(current);
    if (allIn) {
        bool allInOk = (ac.orphanedChips == 0);
        printf("    current: cash=%d set=%d action=ALLIN | orphaned=%d [%s]\n",
               current.cash, current.set, ac.orphanedChips,
               allInOk ? "PASS (all-in unaffected)" : "FAIL");
        if (!allInOk) ok = false;
    } else {
        bool bugReproduced = (ac.orphanedChips > 0) &&
                             (current.cash == startCash) &&
                             (ac.computedContribution == 0);
        printf("    current: cash=%d set=%d action=BET   | pot=%d contribution=%d orphaned=%d [%s]\n",
               current.cash, current.set, ac.chipsInPot, ac.computedContribution,
               ac.orphanedChips, bugReproduced ? "bug reproduced" : "NO DIVERGENCE");
        if (!bugReproduced) ok = false;
    }

    return ok;
}

int main()
{
    int failures = 0;

    printf("Testing missing myCash debit on AI turn/river BET (chip conservation):\n\n");

    // A normal turn bet: 30 chips wagered from a 1000-chip stack.
    if (!runScenario("turn bet", 1000, 30, false)) ++failures;
    // The same code path is reached on the river.
    if (!runScenario("river bet", 1000, 60, false)) ++failures;
    // A larger pot-relative bet, and a small blind-sized bet.
    if (!runScenario("large bet", 500, 200, false)) ++failures;
    if (!runScenario("min bet", 1000, 10, false)) ++failures;
    // The all-in branch must be unchanged by the fix (cash fully committed).
    if (!runScenario("all-in", 1000, 2000, true)) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: missing BET cash debit reproduced and fix verified\n");
    return 0;
}
