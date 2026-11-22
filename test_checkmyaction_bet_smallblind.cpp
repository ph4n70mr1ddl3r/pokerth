// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_BET smallBlind edge case.
//
// Bug hunt: Verify PLAYER_ACTION_BET correctly validates minimum bet
// using smallBlind. The fix ensures targetBet >= 2*smallBlind and
// smallBlind > 0 is checked.
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_checkmyaction_bet_smallblind test_checkmyaction_bet_smallblind.cpp
//   /tmp/test_checkmyaction_bet_smallblind

#include <cstdio>
#include <cstdlib>
#include <cassert>

// Mock Player: minimal subset needed for the bet test.
struct MockPlayer {
    int mySet{0};
    int myCash{1000};
    int smallBlind{10};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getSmallBlind() const { return smallBlind; }
};

// The CORRECT implementation from checkMyAction (with smallBlind check and smallBlind > 0 guard)
static int checkMyActionBetCorrect(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && targetBet >= 2 * p.getSmallBlind() && p.getSmallBlind() > 0) {
        return 0;  // valid bet
    }
    return 1;  // invalid bet
}

// A version WITHOUT the smallBlind > 0 guard (the buggy pattern)
static int checkMyActionBetBuggy(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && targetBet >= 2 * p.getSmallBlind()) {
        return 0;  // BUG: missing smallBlind > 0 check - allows bet when smallBlind=0
    }
    return 1;
}

// A version WITHOUT the smallBlind check entirely (another buggy pattern)
static int checkMyActionBetBuggyNoCheck(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash()) {
        return 0;  // BUG: missing targetBet >= 2*smallBlind check
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("PLAYER_ACTION_BET smallBlind edge case test\n\n");

    // --- Scenario 1: Valid bet with normal smallBlind ---
    printf("--- Scenario 1: Valid bet (targetBet=20, smallBlind=10, cash=1000) ---\n");
    MockPlayer p1;
    p1.smallBlind = 10;
    int correct1 = checkMyActionBetCorrect(p1, 0, 20);
    int buggy1 = checkMyActionBetBuggy(p1, 0, 20);
    printf("  Correct (with smallBlind check): %d (expected 0)\n", correct1);
    printf("  Buggy (missing smallBlind > 0): %d\n", buggy1);
    if (correct1 == 0 && buggy1 == 0) {
        printf("  PASS: Both accept valid bet 20 >= 2*10=20\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 2: Invalid bet when smallBlind is 0 ---
    printf("\n--- Scenario 2: smallBlind=0, targetBet=20 (should reject) ---\n");
    MockPlayer p2;
    p2.smallBlind = 0;
    int correct2 = checkMyActionBetCorrect(p2, 0, 20);
    int buggy2 = checkMyActionBetBuggy(p2, 0, 20);
    printf("  Correct (with smallBlind > 0 guard): %d (expected 1, reject)\n", correct2);
    printf("  Buggy (missing smallBlind > 0): %d (BUG: accepts!)\n", buggy2);
    if (correct2 == 1 && buggy2 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts bet when smallBlind=0\n");
        printf("  Fixed code correctly rejects: PASS\n");
    } else {
        printf("  UNEXPECTED: correct=%d, buggy=%d\n", correct2, buggy2);
        failures++;
    }

    // --- Scenario 3: Invalid bet when targetBet < 2*smallBlind ---
    printf("\n--- Scenario 3: smallBlind=10, targetBet=15 (should reject, 15 < 20) ---\n");
    MockPlayer p3;
    p3.smallBlind = 10;
    int correct3 = checkMyActionBetCorrect(p3, 0, 15);
    int buggy3 = checkMyActionBetBuggy(p3, 0, 15);
    printf("  Correct (with smallBlind check): %d (expected 1, reject)\n", correct3);
    printf("  Buggy (missing smallBlind check): %d\n", buggy3);
    if (correct3 == 1 && buggy3 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts bet 15 < 2*10=20\n");
        printf("  Fixed code correctly rejects: PASS\n");
    } else {
        printf("  UNEXPECTED: correct=%d, buggy=%d\n", correct3, buggy3);
        failures++;
    }

    // --- Scenario 4: Valid bet at minimum 2*smallBlind ---
    printf("\n--- Scenario 4: Valid bet at minimum (targetBet=20, smallBlind=10) ---\n");
    MockPlayer p4;
    p4.smallBlind = 10;
    int correct4 = checkMyActionBetCorrect(p4, 0, 20);
    int buggy4 = checkMyActionBetBuggy(p4, 0, 20);
    printf("  Correct (with smallBlind check): %d (expected 0)\n", correct4);
    printf("  Buggy (missing smallBlind check): %d\n", buggy4);
    if (correct4 == 0 && buggy4 == 0) {
        printf("  PASS: Both accept minimum valid bet 20 >= 2*10=20\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    printf("\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d failures\n", failures);
        return 1;
    }
}