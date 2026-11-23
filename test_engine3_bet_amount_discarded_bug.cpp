// Regression test: the postflop AI engines (flopEngine3/turnEngine3/riverEngine3)
// discard their computed bet amount and always wager exactly 2 * smallBlind.
//
// Bug hunt
// --------
// src/engine/local_engine/localplayer.cpp, flopEngine3() line 4324,
// turnEngine3() line 4522 and riverEngine3() line 4712:
//
//     if (bluff <= 5) mySet = (bluff + 1) * smallBlind;
//     else if (bluff <= 15) mySet = 4 * smallBlind;
//     else mySet = std::max(0, (potential - 1) * 2 * smallBlind);
//
//     if (mySet >= myCash) { ... all in ... }
//     else {
//         int bet = smallBlind * 2;   // <-- computed mySet thrown away
//         myCash -= bet;
//         mySet = oldMySet + bet;
//         myAction = PLAYER_ACTION_BET;
//     }
//
// The computed mySet (which encodes hand strength / bluff) is used only for
// the all-in comparison and then overwritten. Every non-all-in BET is the
// table minimum, and setMinimumRaise() is fed 2*SB instead of the intended
// bet, corrupting raise validation for the other players.
//
// Commit 680b75d introduced this while trying to fix a myCash/mySet update
// bug in preflopEngine(); the "minimum 2*SB" rule is a preflop rule and does
// not belong in the postflop engines.
//
// The fix
// -------
// Restore `int bet = mySet;` in all three engines. This test verifies the
// model behaviour for flop/turn/river scenarios and checks the real source
// file so the regression cannot silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_engine3_bet_amount_discarded_bug test_engine3_bet_amount_discarded_bug.cpp
//   /tmp/test_engine3_bet_amount_discarded_bug

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

struct MockPlayer {
    int mySet{0};
    int myCash{0};
    int oldMySet{0};
    int highestSet{0};
    int minimumRaise{0};
};

// Faithful copy of the bet-size heuristic shared by the three Engine3 functions
// (localplayer.cpp lines 4305-4309 / 4503-4507 / 4693-4697).
static int computeBetAmount(int bluff, int potential, int smallBlind)
{
    if (bluff <= 5) return (bluff + 1) * smallBlind;
    if (bluff <= 15) return 4 * smallBlind;
    return std::max(0, (potential - 1) * 2 * smallBlind);
}

struct Result {
    int wagered{0};
    int myCash{0};
    int mySet{0};
    int minimumRaise{0};
    bool allIn{false};
};

// `fixed` selects `int bet = mySet;`, otherwise the buggy `2 * smallBlind`.
static Result betBranch(int bluff, int potential, int smallBlind, int cash, bool fixed)
{
    MockPlayer p;
    p.myCash = cash;
    p.oldMySet = 0;
    p.highestSet = 0;
    p.mySet = computeBetAmount(bluff, potential, smallBlind);

    Result r;
    if (p.mySet >= p.myCash) {
        r.allIn = true;
        r.wagered = p.myCash;
        p.mySet = p.myCash;
        p.myCash = 0;
    } else {
        const int bet = fixed ? p.mySet : smallBlind * 2;
        r.wagered = bet;
        p.myCash -= bet;
        p.mySet = p.oldMySet + bet;
    }
    if (p.mySet > p.highestSet) p.minimumRaise = p.mySet - p.highestSet;

    r.myCash = p.myCash;
    r.mySet = p.mySet;
    r.minimumRaise = p.minimumRaise;
    return r;
}

struct Case {
    const char *street;
    int bluff;
    int potential;
    int smallBlind;
    int cash;
};

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/engine/local_engine/localplayer.cpp",
        "../src/engine/local_engine/localplayer.cpp",
        "../../src/engine/local_engine/localplayer.cpp",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return "";
}

static int countOccurrences(const std::string &text, const std::string &needle)
{
    int count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

int main()
{
    const Case cases[] = {
        { "flop",  16, 8, 10, 1000 },  // computed 140, buggy 20
        { "flop",  20, 6, 25, 1000 },  // computed 250, buggy 50
        { "turn",  11, 7, 10, 1000 },  // computed 120, buggy 20
        { "turn",  16, 5, 20, 900  },  // computed 160, buggy 40
        { "river", 16, 9, 10, 1000 },  // computed 160, buggy 20
        { "river", 3,  6, 50, 2000 },  // computed 200, buggy 100
    };

    int failures = 0;
    std::printf("Engine3 discarded bet amount regression test\n\n");

    for (const Case &c : cases) {
        const int expected = computeBetAmount(c.bluff, c.potential, c.smallBlind);
        const Result buggy = betBranch(c.bluff, c.potential, c.smallBlind, c.cash, false);
        const Result fixed = betBranch(c.bluff, c.potential, c.smallBlind, c.cash, true);

        std::printf("%-5s SB=%3d bluff=%2d potential=%d expected=%4d | buggy bet=%4d fixed bet=%4d | "
                    "buggy mySet=%4d fixed mySet=%4d | buggy minRaise=%4d fixed minRaise=%4d\n",
                    c.street, c.smallBlind, c.bluff, c.potential, expected,
                    buggy.wagered, fixed.wagered,
                    buggy.mySet, fixed.mySet,
                    buggy.minimumRaise, fixed.minimumRaise);

        if (expected <= c.smallBlind * 2) {
            std::printf("  FAIL: test case does not distinguish the two implementations\n");
            ++failures;
            continue;
        }
        if (buggy.wagered != c.smallBlind * 2 || fixed.wagered != expected) {
            std::printf("  FAIL: bet amount not modelled correctly\n");
            ++failures;
        }
        if (fixed.myCash != c.cash - expected || fixed.mySet != expected) {
            std::printf("  FAIL: fixed path did not update myCash/mySet correctly\n");
            ++failures;
        }
        if (fixed.myCash + fixed.mySet != c.cash) {
            std::printf("  FAIL: chip conservation violated\n");
            ++failures;
        }
        if (fixed.minimumRaise != expected) {
            std::printf("  FAIL: fixed path published the wrong minimum raise\n");
            ++failures;
        }
    }

    // The all-in decision is made from the computed amount in both versions.
    {
        const Result buggy = betBranch(16, 8, 10, 100, false);
        const Result fixed = betBranch(16, 8, 10, 100, true);
        if (!buggy.allIn || !fixed.allIn || buggy.wagered != 100 || fixed.wagered != 100) {
            std::printf("FAIL: all-in path must be identical in both versions\n");
            ++failures;
        }
    }

    // Guard the actual source so the regression cannot creep back in.
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("NOTE: localplayer.cpp not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string good = "int bet = mySet;";
        const std::string bad = "int bet = currentHand->getCurrentBeRo()->getSmallBlind() * 2;";
        const int goodCount = countOccurrences(src, good);
        const int badCount = countOccurrences(src, bad);
        std::printf("\nSource check (%s): 'int bet = mySet;' x%d, hardcoded 2*SB x%d\n",
                    path.c_str(), goodCount, badCount);
        if (goodCount != 3 || badCount != 0) {
            std::printf("  FAIL: expected 3 computed bets and no hardcoded 2*SB bet\n");
            ++failures;
        }
    }

    std::printf("\n");
    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
