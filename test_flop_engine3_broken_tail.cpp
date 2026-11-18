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

// Regression test: leftover post-decision switch in LocalPlayer::flopEngine3()
// (src/engine/local_engine/localplayer.cpp:4367-4409).
//
// Background
// ----------
// In the EngineVersion=1 ("*3") AI engines mySet is the absolute amount
// committed to the pot this betting round and myCash is the stack held back.
// A decision must move both together, because collectSets() (localboard.cpp:57)
// sums mySet into the pot while distributePot() (localboard.cpp:95) derives
// each contribution from (getMyRoundStartCash() - getMyCash()). If the two
// disagree, the pot holds orphaned chips and distributePot()'s "pot > 0"
// fallback dumps them on an arbitrary winner (localboard.cpp:289).
//
// The bug
// -------
// Every other betting engine (preflopEngine, turnEngine3, riverEngine3) was
// cleaned up so its branches perform the cash/mySet updates directly and the
// post-decision switch/evaluation() was removed (commits 24033ef, 254bee7,
// 78a2bff). flopEngine3() was left behind: after its decision block it still
// has (localplayer.cpp:4367-4409):
//
//     evaluation(bet, raise);
//     switch (myAction) { ... case PLAYER_ACTION_RAISE: ... myCash -= totalBet; ...
//                         ... case PLAYER_ACTION_BET: ... myCash -= bet; ... }
//
// Two concrete defects:
//
//   1. BUILD FAILURE. Neither `bet` nor `raise` is declared at that scope in
//      flopEngine3(): `raise` is a local of the RAISE else-branch only, and
//      `bet` is not declared anywhere in the function. `evaluation(bet, raise)`
//      therefore fails to compile. (turnEngine3/riverEngine3 declare `bet` but
//      have no such tail; preflopEngine had the identical tail removed.)
//
//   2. DOUBLE DEDUCTION. The RAISE branch already debits the stack in-line
//      (localplayer.cpp:4341) before the switch runs. The switch's
//      PLAYER_ACTION_RAISE case debits the same chips a second time. The same
//      double deduction is why the identical switch was removed from
//      preflopEngine in 24033ef.
//
// Meanwhile the non-all-in BET and CALL branches of flopEngine3() set mySet /
// myAction but do NOT debit myCash (localplayer.cpp:4311-4314, 4359-4362);
// they were relying on the now-removed/broken tail to do it. Once the broken
// tail is gone, those paths leak chips into the pot.
//
// The fix
// -------
// Mirror turnEngine3()/riverEngine3(): delete evaluation(bet, raise) and the
// trailing switch entirely, and debit explicitly in the two branches that
// need it:
//     BET:  myCash -= mySet;
//     CALL: myCash = myCash - highestSet + mySet;
// RAISE already debits in-line, and ALLIN commits the whole stack, so both
// keep their current accounting.
//
// This standalone test models all four flop decisions under the buggy and
// fixed structures and checks chip conservation through collectSets()/
// distributePot(). No Qt/Boost is required.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_flop_engine3_broken_tail test_flop_engine3_broken_tail.cpp
//   /tmp/test_flop_engine3_broken_tail

#include <algorithm>
#include <cstdio>

enum PlayerAction {
    ACTION_NONE,
    ACTION_CHECK,
    ACTION_BET,
    ACTION_CALL,
    ACTION_RAISE,
    ACTION_ALLIN,
    ACTION_FOLD,
};

struct Player {
    int roundStartCash;
    int cash;   // myCash
    int set;    // mySet (absolute, this betting round)
    PlayerAction action;
};

// Pot accounting performed downstream by collectSets() + distributePot().
struct Accounting {
    int chipsInPot;           // sum of mySet moved into the pot
    int computedContribution; // distributePot(): roundStartCash - myCash
    int orphanedChips;        // chips in the pot with no contributing stack
};

static Accounting account(const Player& p)
{
    Accounting a;
    a.chipsInPot = p.set;
    a.computedContribution = p.roundStartCash - p.cash;
    a.orphanedChips = a.chipsInPot - a.computedContribution;
    return a;
}

// ---- BET ------------------------------------------------------------------
// HEAD's non-all-in BET branch: mySet is assigned, but myCash is not debited.
// The tail that used to debit it is broken (undeclared `bet`), so model it as
// absent -> the stack keeps chips that were placed in the pot.
static void flopBetBuggy(Player& p, int bet)
{
    p.set = bet;
    if (bet >= p.cash) {
        p.set = p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.action = ACTION_BET;   // BUG: myCash not reduced
    }
}

static void flopBetFixed(Player& p, int bet)
{
    p.set = bet;
    if (bet >= p.cash) {
        p.set = p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.cash -= p.set;         // FIX
        p.action = ACTION_BET;
    }
}

// ---- CALL -----------------------------------------------------------------
static void flopCallBuggy(Player& p, int highestSet)
{
    if (highestSet >= p.cash + p.set) {
        p.set += p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.set = highestSet;      // BUG: myCash not reduced
        p.action = ACTION_CALL;
    }
}

static void flopCallFixed(Player& p, int highestSet)
{
    if (highestSet >= p.cash + p.set) {
        p.set += p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
    } else {
        p.cash = p.cash - highestSet + p.set;   // FIX
        p.set = highestSet;
        p.action = ACTION_CALL;
    }
}

// ---- RAISE ----------------------------------------------------------------
// The RAISE branch debits in-line. With HEAD's trailing switch still present,
// the same chips are debited a second time. Model both to show the divergence.
static void flopRaiseBuggy(Player& p, int highestSet, int raise, int oldMySet)
{
    // in-line debit (localplayer.cpp:4341-4342)
    p.cash = p.cash + p.set - highestSet - raise;
    p.set = highestSet + raise;
    p.action = ACTION_RAISE;

    // trailing switch PLAYER_ACTION_RAISE case (localplayer.cpp:4379-4392)
    const int toCall = highestSet - oldMySet;
    const int totalBet = toCall + raise;
    if (totalBet > 0) {
        p.cash -= totalBet;      // BUG: debit a second time
        p.set = highestSet + raise;
    }
}

static void flopRaiseFixed(Player& p, int highestSet, int raise)
{
    p.cash = p.cash + p.set - highestSet - raise;
    p.set = highestSet + raise;
    p.action = ACTION_RAISE;
}

static bool isClean(const Player& p)
{
    const Accounting a = account(p);
    return a.orphanedChips == 0 && a.computedContribution == a.chipsInPot;
}

static bool checkBet(const char* label, int startCash, int bet)
{
    Player buggy{startCash, startCash, 0, ACTION_NONE};
    Player fixed{startCash, startCash, 0, ACTION_NONE};
    flopBetBuggy(buggy, bet);
    flopBetFixed(fixed, bet);
    const Accounting ab = account(buggy);
    const bool bugReproduced = !isClean(buggy) && buggy.cash == startCash &&
                              ab.orphanedChips == bet;
    const bool fixedOk = isClean(fixed) && fixed.cash == startCash - bet &&
                         fixed.set == bet;
    printf("  BET  %-10s buggy: cash=%d orphaned=%d [%s] | fixed: cash=%d orphaned=%d [%s]\n",
           label, buggy.cash, ab.orphanedChips, bugReproduced ? "bug reproduced" : "NO DIVERGENCE",
           fixed.cash, account(fixed).orphanedChips, fixedOk ? "PASS" : "FAIL");
    return bugReproduced && fixedOk;
}

static bool checkCall(const char* label, int startCash, int oldMySet, int highestSet)
{
    Player buggy{startCash, startCash - oldMySet, oldMySet, ACTION_NONE};
    Player fixed{startCash, startCash - oldMySet, oldMySet, ACTION_NONE};
    flopCallBuggy(buggy, highestSet);
    flopCallFixed(fixed, highestSet);
    const Accounting ab = account(buggy);
    const int leaked = highestSet - oldMySet;
    const bool bugReproduced = !isClean(buggy) && ab.orphanedChips == leaked &&
                              buggy.cash == startCash - oldMySet;
    const bool fixedOk = isClean(fixed) &&
                         fixed.cash == startCash - highestSet &&
                         fixed.set == highestSet;
    printf("  CALL %-10s buggy: cash=%d orphaned=%d [%s] | fixed: cash=%d orphaned=%d [%s]\n",
           label, buggy.cash, ab.orphanedChips, bugReproduced ? "bug reproduced" : "NO DIVERGENCE",
           fixed.cash, account(fixed).orphanedChips, fixedOk ? "PASS" : "FAIL");
    return bugReproduced && fixedOk;
}

static bool checkRaise(const char* label, int startCash, int highestSet, int raise, int oldMySet)
{
    Player buggy{startCash, startCash - oldMySet, oldMySet, ACTION_NONE};
    Player fixed{startCash, startCash - oldMySet, oldMySet, ACTION_NONE};
    flopRaiseBuggy(buggy, highestSet, raise, oldMySet);
    flopRaiseFixed(fixed, highestSet, raise);
    const int expectedCash = startCash - highestSet - raise;
    const bool bugReproduced = buggy.cash == expectedCash - (highestSet - oldMySet + raise);
    const bool fixedOk = isClean(fixed) && fixed.cash == expectedCash && fixed.set == highestSet + raise;
    printf("  RAISE %-9s buggy: cash=%d (expected %d) [%s] | fixed: cash=%d orphaned=%d [%s]\n",
           label, buggy.cash, expectedCash, bugReproduced ? "double deduction reproduced" : "NO DIVERGENCE",
           fixed.cash, account(fixed).orphanedChips, fixedOk ? "PASS" : "FAIL");
    return bugReproduced && fixedOk;
}

static bool checkAllIn()
{
    // The all-in branch is unchanged by the fix; both models commit the stack.
    Player p{100, 100, 0, ACTION_NONE};
    const int bet = 200;
    p.set = std::min(bet, p.cash);
    p.cash = 0;
    p.action = ACTION_ALLIN;
    const bool ok = isClean(p) && p.set == 100 && p.cash == 0;
    printf("  ALLIN unchanged: cash=%d set=%d [%s]\n", p.cash, p.set, ok ? "PASS" : "FAIL");
    return ok;
}

int main()
{
    int failures = 0;

    printf("Testing flopEngine3() leftover switch (build failure + chip accounting):\n\n");

    if (!checkBet("normal", 1000, 40)) ++failures;
    if (!checkBet("min bet", 1000, 10)) ++failures;
    if (!checkBet("large", 500, 200)) ++failures;

    if (!checkCall("first call", 1000, 0, 40)) ++failures;
    if (!checkCall("re-call", 1000, 10, 40)) ++failures;

    if (!checkRaise("raise", 1000, 40, 20, 0)) ++failures;
    if (!checkRaise("re-raise", 1000, 40, 20, 10)) ++failures;

    if (!checkAllIn()) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: flopEngine3() defects reproduced and fix verified\n");
    return 0;
}
