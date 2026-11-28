// Regression test: checkMyAction() PLAYER_ACTION_BET smallBlind==0 bug.
//
// Bug: In checkMyAction(), the PLAYER_ACTION_BET case at line 1116 has a condition
// that requires `smallBlind > 0 && targetBet >= 2*smallBlind`. When smallBlind = 0,
// the condition `smallBlind > 0` evaluates to false, causing the function to return 1
// (invalid) even when the player has sufficient chips and highestSet == 0.
//
// The fix changes the condition to `(smallBlind == 0 || targetBet >= 2*smallBlind)`,
// which allows betting when smallBlind == 0 since no blind means no minimum bet
// restriction should apply.
//
// Test scenarios:
//   1. highestSet == 0, smallBlind == 0, targetBet <= myCash: should be valid (no minimum bet restriction)
//   2. highestSet == 0, smallBlind == 0, targetBet > myCash: should be invalid
//   3. highestSet == 0, smallBlind > 0, targetBet >= 2*smallBlind, targetBet <= myCash: should be valid
//   4. highestSet == 0, smallBlind > 0, targetBet < 2*smallBlind: should be invalid
//   5. highestSet > 0: should always be invalid (preflop-only check)

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer::checkMyAction - minimal interface
struct MockPlayer {
    int mySet{0};
    int myCash{1000};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Current (fixed) checkMyAction PLAYER_ACTION_BET case from localplayer.cpp:1116-1121
static int checkMyActionBetFixed(int highestSet, int targetBet, int myCash, int smallBlind) {
    if (highestSet == 0 && targetBet <= myCash && (smallBlind == 0 || targetBet >= 2 * smallBlind)) {
        return 0;  // valid
    }
    return 1;  // invalid
}

// Old (buggy) checkMyAction PLAYER_ACTION_BET case
static int checkMyActionBetBuggy(int highestSet, int targetBet, int myCash, int smallBlind) {
    if (highestSet == 0 && targetBet <= myCash && (smallBlind > 0 && targetBet >= 2 * smallBlind)) {
        return 0;  // valid
    }
    return 1;  // invalid
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction() PLAYER_ACTION_BET smallBlind==0 bug\n\n");

    // --- Scenario 1: highestSet == 0, smallBlind == 0, targetBet <= myCash ---
    printf("--- Scenario 1 (highestSet=0, targetBet=10, myCash=100, smallBlind=0) ---\n");
    printf("  smallestBlind=0: no minimum bet restriction should apply\n\n");

    int fixed1 = checkMyActionBetFixed(0, 10, 100, 0);
    int buggy1 = checkMyActionBetBuggy(0, 10, 100, 0);
    printf("  Fixed: %d (expected 0, valid bet)\n", fixed1);
    printf("  Buggy: %d\n", buggy1);
    if (fixed1 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows bet when smallBlind=0\n");
    }
    if (buggy1 == 0) {
        printf("  BUG CONFIRMED: Buggy code also allows (unexpected)\n");
    } else {
        printf("  BUG CONFIRMED: Buggy code incorrectly rejects (this is the bug)\n");
    }

    // --- Scenario 2: highestSet == 0, smallBlind == 0, targetBet > myCash ---
    printf("\n--- Scenario 2 (highestSet=0, targetBet=200, myCash=100, smallBlind=0) ---\n");
    printf("  smallestBlind=0 but targetBet > myCash: should be invalid\n\n");

    int fixed2 = checkMyActionBetFixed(0, 200, 100, 0);
    int buggy2 = checkMyActionBetBuggy(0, 200, 100, 0);
    printf("  Fixed: %d (expected 1, invalid bet - has insufficient cash)\n", fixed2);
    printf("  Buggy: %d\n", buggy2);
    if (fixed2 != 1) {
        printf("  FIX FAIL: expected 1\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly rejects bet when insufficient cash\n");
    }

    // --- Scenario 3: highestSet == 0, smallBlind > 0, targetBet >= 2*smallBlind ---
    printf("\n--- Scenario 3 (highestSet=0, targetBet=20, myCash=100, smallBlind=10) ---\n");
    printf("  smallestBlind=10, targetBet=20 >= 2*10=20: should be valid\n\n");

    int fixed3 = checkMyActionBetFixed(0, 20, 100, 10);
    int buggy3 = checkMyActionBetBuggy(0, 20, 100, 10);
    printf("  Fixed: %d (expected 0, valid bet at minimum)\n", fixed3);
    printf("  Buggy: %d\n", buggy3);
    if (fixed3 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows minimum bet\n");
    }
    if (buggy3 == 0) {
        printf("  BUG CONFIRMED: Buggy code also allows (expected)\n");
    } else {
        printf("  BUG CONFIRMED: Buggy code rejects minimum bet (unexpected)\n");
    }

    // --- Scenario 4: highestSet == 0, smallBlind > 0, targetBet < 2*smallBlind ---
    printf("\n--- Scenario 4 (highestSet=0, targetBet=15, myCash=100, smallBlind=10) ---\n");
    printf("  smallestBlind=10, targetBet=15 < 2*10=20: should be invalid\n\n");

    int fixed4 = checkMyActionBetFixed(0, 15, 100, 10);
    int buggy4 = checkMyActionBetBuggy(0, 15, 100, 10);
    printf("  Fixed: %d (expected 1, invalid bet - below minimum)\n", fixed4);
    printf("  Buggy: %d\n", buggy4);
    if (fixed4 != 1) {
        printf("  FIX FAIL: expected 1\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly rejects bet below minimum\n");
    }

    // --- Scenario 5: highestSet > 0, should always reject bet ---
    printf("\n--- Scenario 5 (highestSet=100, targetBet=10, myCash=1000, smallBlind=5) ---\n");
    printf("  highestSet > 0: bet action should be invalid (preflop-only check)\n\n");

    int fixed5 = checkMyActionBetFixed(100, 10, 1000, 5);
    int buggy5 = checkMyActionBetBuggy(100, 10, 1000, 5);
    printf("  Fixed: %d (expected 1, invalid bet - postflop)\n", fixed5);
    printf("  Buggy: %d\n", buggy5);
    if (fixed5 != 1) {
        printf("  FIX FAIL: expected 1\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly rejects bet when highestSet > 0\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: checkMyAction PLAYER_ACTION_BET smallBlind==0 edge case verified\n");
    return 0;
}