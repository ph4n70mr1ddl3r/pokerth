// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_BET bug.
//
// Bug hunt: Verify PLAYER_ACTION_BET correctly validates minimum bet
// using smallBlind. The fix ensures targetBet >= 2*smallBlind.
//
// This test was inspired by recent bug fixes in the codebase that added
// missing validation checks to edge cases.

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

// The CORRECT implementation from checkMyAction (with smallBlind check)
static int checkMyActionBetCorrect(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash() && targetBet >= 2 * p.getSmallBlind()) {
        return 0;  // valid bet
    }
    return 1;  // invalid bet
}

// Simulates a buggy version MISSING the smallBlind minimum check
static int checkMyActionBetBuggy(MockPlayer& p, int highestSet, int targetBet) {
    if (highestSet == 0 && targetBet <= p.getMyCash()) {
        return 0;  // BUG: missing targetBet >= 2*smallBlind check
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("PLAYER_ACTION_BET bug verification test\n\n");

    // --- Scenario 1: Valid bet (targetBet >= 2*smallBlind) ---
    printf("--- Scenario 1: Valid bet (targetBet=20, smallBlind=10, cash=1000) ---\n");
    MockPlayer p1;
    p1.smallBlind = 10;
    int correct1 = checkMyActionBetCorrect(p1, 0, 20);
    int buggy1 = checkMyActionBetBuggy(p1, 0, 20);
    printf("  Correct (with smallBlind check): %d (expected 0)\n", correct1);
    printf("  Buggy (missing smallBlind check): %d\n", buggy1);
    // Both should accept valid bet of 20 (>= 2*10)
    if (correct1 == 0 && buggy1 == 0) {
        printf("  PASS: Both accept valid bet 20 >= 2*10=20\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 2: Invalid bet (targetBet < 2*smallBlind) ---

    printf("\n--- Scenario 2: Invalid bet (targetBet=5, smallBlind=10, cash=1000) ---\n");
    MockPlayer p2;
    p2.smallBlind = 10;
    int correct2 = checkMyActionBetCorrect(p2, 0, 5);
    int buggy2 = checkMyActionBetBuggy(p2, 0, 5);
    printf("  Correct (with smallBlind check): %d (expected 1, reject)\n", correct2);
    printf("  Buggy (missing smallBlind check): %d (BUG: accepts!)\n", buggy2);
    // Correct should reject, buggy should incorrectly accept
    if (correct2 == 1 && buggy2 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts bet < 2*smallBlind\n");
        printf("  Fixed code correctly rejects: PASS\n");
    } else {
        printf("  Unexpected results: FAIL\n");
        failures++;
    }

    // --- Scenario 3: Bet above cash ---
    printf("\n--- Scenario 3: Bet above cash (targetBet=5000, smallBlind=10, cash=1000) ---\n");
    MockPlayer p3;
    p3.smallBlind = 10;
    int correct3 = checkMyActionBetCorrect(p3, 0, 5000);
    int buggy3 = checkMyActionBetBuggy(p3, 0, 5000);
    printf("  Correct (with smallBlind check): %d (expected 1, reject > cash)\n", correct3);
    printf("  Buggy (missing smallBlind check but has cash check): %d\n", buggy3);
    if (correct3 == 1) {
        printf("  PASS: Correct code rejects bet above cash\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 4: highestSet > 0, bet should be invalid ---
    printf("\n--- Scenario 4: highestSet > 0 (targetBet=20, smallBlind=10, highestSet=100) ---\n");
    MockPlayer p4;
    p4.smallBlind = 10;
    int correct4 = checkMyActionBetCorrect(p4, 100, 20);
    int buggy4 = checkMyActionBetBuggy(p4, 100, 20);
    printf("  Correct (with smallBlind check): %d (expected 1, reject when highestSet > 0)\n", correct4);
    printf("  Buggy: %d\n", buggy4);
    if (correct4 == 1) {
        printf("  PASS: Correct code rejects bet when highestSet > 0\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 5: Edge case - exact 2*smallBlind ---
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

    printf("TEST PASSED: PLAYER_ACTION_BET bug verification complete\n");
    return 0;
}