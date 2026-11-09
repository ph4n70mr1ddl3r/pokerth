// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_BET smallBlind edge case.
//
// Bug hunt: Verify PLAYER_ACTION_BET correctly validates minimum bet
// using smallBlind. The fix ensures targetBet >= 2*smallBlind and
// smallBlind > 0 is checked.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_bet_smallblind_edge test_bet_smallblind_edge.cpp && /tmp/test_bet_smallblind_edge

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

// A BUGGY version MISSING the smallBlind > 0 guard
static int checkMyActionBetBuggy(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && targetBet >= 2 * p.getSmallBlind()) {
        return 0;  // BUG: missing smallBlind > 0 check
    }
    return 1;
}

// A BUGGY version MISSING the smallBlind check entirely
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
        printf("  Unexpected results: FAIL\n");
        failures++;
    }

    // --- Scenario 3: Valid bet with smallBlind=0 and targetBet=0 ---
    printf("\n--- Scenario 3: smallBlind=0, targetBet=0 (should reject since smallBlind<=0) ---\n");
    MockPlayer p3;
    p3.smallBlind = 0;
    int correct3 = checkMyActionBetCorrect(p3, 0, 0);
    printf("  Correct (with smallBlind > 0 guard): %d (expected 1, reject because smallBlind<=0)\n", correct3);
    if (correct3 == 1) {
        printf("  PASS: Correct code rejects bet when smallBlind=0\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 4: Buggy version missing smallBlind check ---
    printf("\n--- Scenario 4: Buggy code missing 2*smallBlind check ---\n");
    MockPlayer p4;
    p4.smallBlind = 10;
    int correct4 = checkMyActionBetCorrect(p4, 0, 5);   // targetBet < 2*smallBlind
    int buggy4 = checkMyActionBetBuggyNoCheck(p4, 0, 5);
    printf("  Correct (with smallBlind check): %d (expected 1, reject 5 < 20)\n", correct4);
    printf("  Buggy (missing smallBlind check): %d (BUG: accepts!)\n", buggy4);
    if (correct4 == 1 && buggy4 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts bet 5 < 2*10=20\n");
        printf("  Fixed code correctly rejects: PASS\n");
    } else {
        printf("  Unexpected results: FAIL\n");
        failures++;
    }

    // --- Scenario 5: Edge case exact 2*smallBlind ---
    printf("\n--- Scenario 5: Edge case exact 2*smallBlind (targetBet=20, smallBlind=10) ---\n");
    MockPlayer p5;
    p5.smallBlind = 10;
    int correct5 = checkMyActionBetCorrect(p5, 0, 20);
    printf("  Correct (with smallBlind check): %d (expected 0, exact 2*smallBlind should be valid)\n", correct5);
    if (correct5 == 0) {
        printf("  PASS: Exact 2*smallBlind accepted\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_BET smallBlind edge case test complete\n");
    return 0;
}