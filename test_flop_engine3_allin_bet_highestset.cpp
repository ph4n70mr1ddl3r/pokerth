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

// Regression test: an all-in BET in LocalPlayer::flopEngine3() never publishes
// the committed chips to the board's highestSet/minimumRaise.
//
// In the EngineVersion=1 ("*3") engines mySet is the absolute amount committed
// this round and myCash the stack held back. A BET is only possible when the
// board's highestSet is still 0. If the intended bet is at least the remaining
// stack, the player is all-in (mySet = myCash; myCash = 0).
//
// turnEngine3() and riverEngine3() publish the resulting bet to the board for
// BOTH the normal BET and the all-in case: the setHighestSet()/setMinimumRaise()
// block sits after the if/else. In flopEngine3() that block is stuck inside the
// non-all-in `else`, so an all-in bet leaves highestSet at 0. Every opponent
// then sees an unbet board in flopEngine3() and takes the free CHECK branch
// (mySet == highestSet == 0) instead of calling, raising or folding. The chips
// still reach the pot via collectSets(), so the pot looks correct while the
// betting round itself is broken.
//
// This standalone test models the flop BET decision, the board publication and
// the next player's reaction under buggy and fixed structures. No Qt/Boost.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_flop_engine3_allin_bet_highestset test_flop_engine3_allin_bet_highestset.cpp
//   /tmp/test_flop_engine3_allin_bet_highestset

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

struct Board {
    int highestSet = 0;
    int minimumRaise = 0;
};

struct Player {
    int cash;
    int set;
    PlayerAction action;
};

// flopEngine3() BET branch: `betIntent` is the computed bet (mySet). The board
// publication is conditional in HEAD (inside the non-all-in else) and
// unconditional after the fix.
static void flopBet(Player& p, Board& b, int betIntent, bool publishAllIn)
{
    p.set = betIntent;
    if (p.set >= p.cash) {
        p.set = p.cash;
        p.cash = 0;
        p.action = ACTION_ALLIN;
        if (publishAllIn) {
            const int oldHighest = b.highestSet;
            b.highestSet = p.set;
            if (p.set > oldHighest) b.minimumRaise = p.set - oldHighest;
        }
    } else {
        p.cash -= p.set;
        p.action = ACTION_BET;
        const int oldHighest = b.highestSet;
        b.highestSet = p.set;
        if (p.set > oldHighest) b.minimumRaise = p.set - oldHighest;
    }
}

// The next player's flopEngine3() split: highestSet == 0 selects the
// CHECK/BET branch; with mySet == 0 that is a free check.
static PlayerAction opponentReaction(int mySet, int highestSet)
{
    if (highestSet == 0) {
        return mySet == highestSet ? ACTION_CHECK : ACTION_BET;
    }
    return ACTION_CALL;
}

static bool checkCase(const char* label, int cash, int betIntent, int expectedSet,
                      PlayerAction expectedAction)
{
    Board buggyBoard;
    Board fixedBoard;
    Player buggy{cash, 0, ACTION_NONE};
    Player fixed{cash, 0, ACTION_NONE};

    flopBet(buggy, buggyBoard, betIntent, false);
    flopBet(fixed, fixedBoard, betIntent, true);

    const bool buggyAllIn = buggy.action == ACTION_ALLIN;
    const bool buggyLeaksBet = buggyAllIn && buggyBoard.highestSet == 0;
    const bool fixedBoardOk = fixedBoard.highestSet == expectedSet &&
                              fixedBoard.minimumRaise == expectedSet;
    const bool stateOk = fixed.cash == cash - expectedSet &&
                         fixed.set == expectedSet &&
                         fixed.action == expectedAction;

    const PlayerAction buggyReaction = opponentReaction(0, buggyBoard.highestSet);
    const PlayerAction fixedReaction = opponentReaction(0, fixedBoard.highestSet);
    const bool consequenceShown = !buggyAllIn ||
                                  (buggyReaction == ACTION_CHECK &&
                                   fixedReaction == ACTION_CALL);

    // Fixed state must be correct; for all-in the buggy board must provably be
    // left at 0 and provably change the opponent's decision.
    const bool pass = stateOk && fixedBoardOk &&
                      (!buggyAllIn || (buggyLeaksBet && consequenceShown));

    printf("  %-18s buggy: action=%d highest=%d opponent=%d | fixed: set=%d cash=%d highest=%d minRaise=%d opponent=%d [%s]\n",
           label, buggy.action, buggyBoard.highestSet, buggyReaction,
           fixed.set, fixed.cash, fixedBoard.highestSet, fixedBoard.minimumRaise,
           fixedReaction, pass ? "PASS" : "FAIL");
    return pass;
}

int main()
{
    int failures = 0;

    printf("Testing flopEngine3() all-in BET board publication:\n\n");

    if (!checkCase("short stack all-in", 30, 40, 30, ACTION_ALLIN)) ++failures;
    if (!checkCase("exact all-in", 40, 40, 40, ACTION_ALLIN)) ++failures;
    if (!checkCase("large all-in", 500, 800, 500, ACTION_ALLIN)) ++failures;
    if (!checkCase("normal bet", 1000, 40, 40, ACTION_BET)) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: all-in BET is published to the board; buggy path verified\n");
    return 0;
}
