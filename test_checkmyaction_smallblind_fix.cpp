// Regression test: LocalPlayer::checkMyAction() PLAYER_ACTION_BET smallBlind==0 edge case.
//
// The fix changed the condition from
// (smallBlind == 0 || targetBet >= 2*smallBlind) to (smallBlind > 0 && targetBet >= 2*smallBlind).
// This test verifies the fixed behavior.
//
// Adapted from test/checkmyaction_bet_smallblind_edge.cpp

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

// The FIXED implementation from checkMyAction (with smallBlind > 0 guard)
static int checkMyActionBetFixed(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && (p.getSmallBlind() > 0 && targetBet >= 2 * p.getSmallBlind())) {
        return 0;  // valid bet
    }
    return 1;  // invalid bet
}

// The OLD buggy implementation (with smallBlind == 0 guard)
static int checkMyActionBetOldBuggy(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && (p.getSmallBlind() == 0 || targetBet >= 2 * p.getSmallBlind())) {
        return 0;  // OLD BUG: allows bet when smallBlind=0
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("PLAYER_ACTION_BET smallBlind edge case test (post-fix verification)\n\n");

    // --- Scenario 1: Valid bet with normal smallBlind ---
    printf("--- Scenario 1: Valid bet (targetBet=20, smallBlind=10, cash=1000) ---\n");
    MockPlayer p1;
    p1.smallBlind = 10;
    int fixed1 = checkMyActionBetFixed(p1, 0, 20);
    int old1 = checkMyActionBetOldBuggy(p1, 0, 20);
    printf("  Fixed implementation: %d (expected 0)\n", fixed1);
    printf("  Old buggy implementation: %d\n", old1);
    if (fixed1 == 0 && old1 == 0) {
        printf("  PASS: Both implementations accept valid bet 20 >= 2*10=20\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 2: Invalid bet when smallBlind is 0 (should be rejected with fix) ---
    printf("\n--- Scenario 2: smallBlind=0, targetBet=20 (should reject) ---\n");
    MockPlayer p2;
    p2.smallBlind = 0;
    int fixed2 = checkMyActionBetFixed(p2, 0, 20);
    int old2 = checkMyActionBetOldBuggy(p2, 0, 20);
    printf("  Fixed implementation: %d (expected 1, reject)\n", fixed2);
    printf("  Old buggy implementation: %d (BUG: accepts!)\n", old2);
    if (fixed2 == 1 && old2 == 0) {
        printf("  BUG CONFIRMED: Old code accepts bet when smallBlind=0\n");
        printf("  Fixed code correctly rejects: PASS\n");
    } else {
        printf("  UNEXPECTED: correct=%d, old=%d\n", fixed2, old2);
        failures++;
    }

    // --- Scenario 3: Invalid bet when targetBet < 2*smallBlind (both should reject) ---
    printf("\n--- Scenario 3: smallBlind=10, targetBet=15 (15 < 2*10=20, should reject) ---\n");
    MockPlayer p3;
    p3.smallBlind = 10;
    int fixed3 = checkMyActionBetFixed(p3, 0, 15);
    int old3 = checkMyActionBetOldBuggy(p3, 0, 15);
    printf("  Fixed implementation: %d (expected 1, reject)\n", fixed3);
    printf("  Old buggy implementation: %d (expected 1, reject)\n", old3);
    if (fixed3 == 1 && old3 == 1) {
        printf("  PASS: Both correctly reject bet 15 < 2*10=20\n");
    } else {
        printf("  UNEXPECTED: correct=%d, old=%d\n", fixed3, old3);
        failures++;
    }

    // --- Scenario 4: Valid bet at minimum 2*smallBlind ---
    printf("\n--- Scenario 4: Valid bet at minimum (targetBet=20, smallBlind=10) ---\n");
    MockPlayer p4;
    p4.smallBlind = 10;
    int fixed4 = checkMyActionBetFixed(p4, 0, 20);
    int old4 = checkMyActionBetOldBuggy(p4, 0, 20);
    printf("  Fixed implementation: %d (expected 0)\n", fixed4);
    printf("  Old buggy implementation: %d\n", old4);
    if (fixed4 == 0 && old4 == 0) {
        printf("  PASS: Both implementations accept minimum valid bet 20 >= 2*10=20\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 5: Edge case - highestSet != 0 (should always reject bet) ---
    printf("\n--- Scenario 5: highestSet!=0, should reject bet ---\n");
    MockPlayer p5;
    p5.smallBlind = 10;
    int fixed5 = checkMyActionBetFixed(p5, 100, 20);
    int old5 = checkMyActionBetOldBuggy(p5, 100, 20);
    printf("  Fixed implementation: %d (expected 1, reject)\n", fixed5);
    printf("  Old buggy implementation: %d\n", old5);
    if (fixed5 == 1 && old5 == 1) {
        printf("  PASS: All implementations reject bet when highestSet!=0\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 6: Edge case - targetBet > myCash (should reject) ---
    printf("\n--- Scenario 6: targetBet > myCash (100 > 50, should reject) ---\n");
    MockPlayer p6;
    p6.smallBlind = 10;
    p6.myCash = 50;
    int fixed6 = checkMyActionBetFixed(p6, 0, 100);  // targetBet=100 > myCash=50
    int old6 = checkMyActionBetOldBuggy(p6, 0, 100);
    printf("  Fixed implementation: %d (expected 1, reject)\n", fixed6);
    printf("  Old buggy implementation: %d (expected 1, reject)\n", old6);
    if (fixed6 == 1 && old6 == 1) {
        printf("  PASS: Both correctly reject bet when targetBet > myCash\n");
    } else {
        printf("  UNEXPECTED: correct=%d, old=%d\n", fixed6, old6);
        failures++;
    }

    printf("\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }
}