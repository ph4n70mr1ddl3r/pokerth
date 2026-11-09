// Test for PLAYER_ACTION_CALL mySet >= highestSet with zero cash bug.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case at line 1074
// returns 0 (valid) when getMySet() >= highestSet and targetBet <= getMyCash(),
// without checking if getMyCash() > 0. When myCash == 0 and targetBet == 0,
// the call is incorrectly accepted because 0 <= 0 is true, even though the
// player has no chips to bet.
//
// Fix: Add getMyCash() > 0 guard to the mySet >= highestSet condition.
//
// This test verifies the bug: without the fix, call accepted when myCash==0;
// with the fix, call correctly rejected when myCash==0.
#include <cstdio>
#include <cstdlib>

// Mock Player: minimal subset needed for the call test.
struct MockPlayer {
    int mySet{0};
    int myCash{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Current (buggy) PLAYER_ACTION_CALL check from localplayer.cpp
static int checkMyActionCallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        // Player has already put in enough chips (mySet >= highestSet).
        // Call is valid only if targetBet is within cash.
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
        return 1;
    }
    return 1;
}

// Fixed PLAYER_ACTION_CALL check: adds getMyCash() > 0 guard
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        // Player has sufficient chips AND non-zero cash - call is valid if targetBet is within cash
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_CALL mySet >= highestSet with zero cash bug\n\n");

    // --- Scenario 1: mySet >= highestSet, myCash == 0, targetBet == 0 (THE BUG) ---
    printf("--- Scenario 1 (mySet=100, highestSet=100, myCash=0, targetBet=0) ---\n");
    MockPlayer p1;
    p1.mySet = 100;
    p1.myCash = 0;
    int buggy1 = checkMyActionCallBuggy(p1, 0, 100);
    int fixed1 = checkMyActionCallFixed(p1, 0, 100);
    printf("  Buggy: %d (expected 1, should reject - no chips!)\n", buggy1);
    printf("  Fixed: %d (expected 1, correctly rejects)\n", fixed1);
    if (buggy1 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts call when myCash == 0\n");
    } else {
        printf("  Bug NOT confirmed: FAIL (buggy unexpectedly rejects)\n");
        failures++;
    }
    if (fixed1 == 1) {
        printf("  Fix works: PASS (correctly rejects invalid call)\n");
    } else {
        printf("  Fix FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 2: Normal case with myCash > 0 ---
    printf("\n--- Scenario 2 (mySet=100, highestSet=100, myCash=50, targetBet=50) ---\n");
    MockPlayer p2;
    p2.mySet = 100;
    p2.myCash = 50;
    int buggy2 = checkMyActionCallBuggy(p2, 50, 100);
    int fixed2 = checkMyActionCallFixed(p2, 50, 100);
    printf("  Buggy: %d (expected 0, valid call)\n", buggy2);
    printf("  Fixed: %d (expected 0, valid call)\n", fixed2);
    if (buggy2 == 0 && fixed2 == 0) {
        printf("  PASS: Both accept valid call when myCash > 0\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 3: mySet >= highestSet, myCash > 0, targetBet > myCash ---
    printf("\n--- Scenario 3 (mySet=100, highestSet=100, myCash=50, targetBet=100) ---\n");
    MockPlayer p3;
    p3.mySet = 100;
    p3.myCash = 50;
    int fixed3 = checkMyActionCallFixed(p3, 100, 100);
    printf("  Fixed: %d (expected 1, invalid since targetBet > myCash)\n", fixed3);
    if (fixed3 == 1) {
        printf("  PASS: Fixed code correctly rejects call when targetBet > myCash\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 4: mySet >= highestSet, myCash == 0, targetBet > 0 ---
    printf("\n--- Scenario 4 (mySet=100, highestSet=100, myCash=0, targetBet=50) ---\n");
    MockPlayer p4;
    p4.mySet = 100;
    p4.myCash = 0;
    int fixed4 = checkMyActionCallFixed(p4, 50, 100);
    printf("  Fixed: %d (expected 1, invalid since myCash==0)\n", fixed4);
    if (fixed4 == 1) {
        printf("  PASS: Fixed code correctly rejects call when myCash == 0\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL mySet >= highestSet with zero cash bug confirmed and fix verified\n");
    return 0;
}