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

// Regression test: the EngineVersion=1 ("*3") AI engines never set fullBetRule
// when they shove all-in with a raise/bet that is short of a full raise.
//
// Poker rule: an all-in that does not raise by at least the previous full raise
// amount does not reopen betting for the players behind. The board therefore
// sets fullBetRule=true so those players may only CHECK-CALL or FOLD. This is
// implemented in evaluation() (EngineVersion=0, localplayer.cpp:2815-2817 and
// 2852-2868) and in the GUI human all-in path (gametableimpl.cpp:1950-1953 and
// 2012-2015). The duplicated preflopEngine3()/flopEngine3()/turnEngine3()/
// riverEngine3() implementations mirror the same decisions but omit the
// setFullBetRule(true) call entirely, so an AI short all-in leaves the board
// open and a human opponent is wrongly offered a re-raise against an opponent
// who can no longer answer it.
//
// This standalone test models the all-in decision in the buggy and fixed
// structures, cross-checks it against the v0 evaluation() reference and the
// GUI behavior, and shows the consequence for the next player. No Qt/Boost.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_fullbetrule_ai_allin test_fullbetrule_ai_allin.cpp
//   /tmp/test_fullbetrule_ai_allin

#include <cstdio>

struct Board {
    int highestSet = 0;
    int minimumRaise = 0;
    int smallBlind = 10;
    bool fullBetRule = false;
};

struct Player {
    int cash;
    int set;
};

// The EngineVersion=1 all-in RAISE branch, as found in all four "*3" engines.
// `setFullBetRule` toggles between HEAD (false) and the fix (true).
static void aiAllInRaise(Player& p, Board& b, int raise, bool setFullBetRule)
{
    if (b.highestSet + raise < p.cash + p.set) return; // not an all-in

    const int oldHighest = b.highestSet;
    const int oldMinimumRaise = b.minimumRaise;
    p.set += p.cash;
    p.cash = 0;

    if (setFullBetRule && p.set > oldHighest && p.set - oldHighest < oldMinimumRaise) {
        b.fullBetRule = true; // short all-in raise: betting stays closed
    }
    if (p.set > oldHighest) {
        b.highestSet = p.set;
        b.minimumRaise = p.set - oldHighest;
    }
}

// The EngineVersion=1 all-in BET branch (flop/turn/river "*3").
static void aiAllInBet(Player& p, Board& b, int betIntent, bool setFullBetRule)
{
    p.set = betIntent;
    if (p.set < p.cash) return; // not an all-in

    p.set = p.cash;
    p.cash = 0;
    if (setFullBetRule && p.set < 2 * b.smallBlind) {
        b.fullBetRule = true; // sub-minimum all-in bet: betting stays closed
    }
    b.highestSet = p.set;
    b.minimumRaise = p.set;
}

// The EngineVersion=0 evaluation() reference for the same short all-in raise.
static void v0AllInRaise(Player& p, Board& b, int raise)
{
    if (b.highestSet + raise < p.cash + p.set) return;
    if (b.highestSet + b.minimumRaise > p.cash + p.set) {
        if (b.highestSet >= p.cash + p.set) {
            p.set += p.cash; // only call all-in
            p.cash = 0;
            return;
        }
        b.fullBetRule = true; // raise, but not enough --> full bet rule
        p.set += p.cash;
        p.cash = 0;
        b.minimumRaise = p.set - b.highestSet;
        b.highestSet = p.set;
        return;
    }
    p.set += p.cash;
    p.cash = 0;
    b.minimumRaise = p.set - b.highestSet;
    b.highestSet = p.set;
}

// gametableimpl.cpp:1536-1540 -- is a re-raise offered to the next player?
static bool nextPlayerMayRaise(const Board& b, int opponentCashAndSet)
{
    return !b.fullBetRule && opponentCashAndSet > b.highestSet;
}

static bool checkRaise(const char* label, int highestSet, int minimumRaise,
                       int cash, int raise, bool expectFullBetRule)
{
    Board buggy{highestSet, minimumRaise, 10, false};
    Board fixed{highestSet, minimumRaise, 10, false};
    Board reference{highestSet, minimumRaise, 10, false};
    Player buggyPlayer{cash, 0};
    Player fixedPlayer{cash, 0};
    Player referencePlayer{cash, 0};

    aiAllInRaise(buggyPlayer, buggy, raise, false);
    aiAllInRaise(fixedPlayer, fixed, raise, true);
    v0AllInRaise(referencePlayer, reference, raise);

    const bool stateOk = fixedPlayer.set == referencePlayer.set &&
                         fixed.highestSet == reference.highestSet &&
                         fixed.minimumRaise == reference.minimumRaise;
    const bool ruleOk = fixed.fullBetRule == expectFullBetRule &&
                        reference.fullBetRule == expectFullBetRule;
    const bool reproducesBug = !expectFullBetRule || !buggy.fullBetRule;
    const bool consequence = !expectFullBetRule ||
                             (nextPlayerMayRaise(buggy, cash + fixedPlayer.set) &&
                              !nextPlayerMayRaise(fixed, cash + fixedPlayer.set));
    const bool pass = stateOk && ruleOk && reproducesBug && consequence;

    printf("  %-24s buggy(fullRule=%d,reRaises=%d) fixed(fullRule=%d,reRaises=%d) v0(fullRule=%d) [%s]\n",
           label, buggy.fullBetRule, nextPlayerMayRaise(buggy, cash + fixedPlayer.set),
           fixed.fullBetRule, nextPlayerMayRaise(fixed, cash + fixedPlayer.set),
           reference.fullBetRule, pass ? "PASS" : "FAIL");
    return pass;
}

static bool checkBet(const char* label, int cash, int betIntent, bool expectFullBetRule)
{
    Board buggy{0, 20, 10, false};
    Board fixed{0, 20, 10, false};
    Player buggyPlayer{cash, 0};
    Player fixedPlayer{cash, 0};

    aiAllInBet(buggyPlayer, buggy, betIntent, false);
    aiAllInBet(fixedPlayer, fixed, betIntent, true);

    const bool isAllIn = betIntent >= cash;
    const bool ruleOk = fixed.fullBetRule == expectFullBetRule;
    const bool reproducesBug = !isAllIn || !expectFullBetRule || !buggy.fullBetRule;
    const bool consequence = !isAllIn || !expectFullBetRule ||
                             (nextPlayerMayRaise(buggy, 100) &&
                              !nextPlayerMayRaise(fixed, 100));
    const bool pass = fixedPlayer.set == buggyPlayer.set &&
                      fixed.highestSet == buggy.highestSet && ruleOk &&
                      reproducesBug && consequence;

    printf("  %-24s buggy(fullRule=%d) fixed(fullRule=%d) expected=%d [%s]\n",
           label, buggy.fullBetRule, fixed.fullBetRule, expectFullBetRule,
           pass ? "PASS" : "FAIL");
    return pass;
}

int main()
{
    int failures = 0;

    printf("Testing fullBetRule on EngineVersion=1 AI short all-ins:\n\n");
    printf("RAISE (board highestSet / minimumRaise, AI stack):\n");

    // AI shoves 150 over a 100 bet whose minimum raise is 80: raise of 50.
    if (!checkRaise("short all-in raise", 100, 80, 150, 100, true)) ++failures;
    // Exactly a full raise: betting reopens, no full bet rule.
    if (!checkRaise("exact full raise", 100, 80, 180, 100, false)) ++failures;
    // Bigger than a full raise: betting reopens.
    if (!checkRaise("over full raise", 100, 80, 300, 100, false)) ++failures;
    // Cannot even match the current bet: call-all-in, no full bet rule.
    if (!checkRaise("call all-in only", 100, 80, 80, 100, false)) ++failures;

    printf("\nBET (unbet board, minimum bet = 2 * small blind = 20):\n");

    if (!checkBet("sub-minimum shove", 15, 40, true)) ++failures;
    if (!checkBet("exactly min bet shove", 20, 40, false)) ++failures;
    if (!checkBet("full bet shove", 100, 150, false)) ++failures;
    if (!checkBet("ordinary bet", 100, 40, false)) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: short AI all-ins close betting via fullBetRule; buggy path verified\n");
    return 0;
}
