// Test for checkMyAction() PLAYER_ACTION_CALL myCash > 0 guard bug.
//
// Bug: In checkMyAction(), the PLAYER_ACTION_CALL case checks
// getMySet() >= highestSet, but does not verify that the player has
// non-zero cash. When myCash == 0 and mySet >= highestSet, the function
// incorrectly returns 0 (valid call) because 0 >= 0 and targetBet <= 0,
// even though the player has no chips to bet.
//
// Fix: Add getMyCash() > 0 guard to the mySet >= highestSet condition.
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_checkmyaction_cashguard_bug test_checkmyaction_cashguard_bug.cpp && /tmp/test_checkmyaction_cashguard_bug

#include <cstdio>
#include <cstdlib>

// Mock Player: minimal subset needed for the call test.
struct MockPlayer {
    int mySet{0};
    int myCash{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Buggy checkMyAction PLAYER_ACTION_CALL: no cash guard
static int checkMyActionCallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        // BUG: no check for myCash > 0
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

// Fixed checkMyAction PLAYER_ACTION_CALL: with cash guard
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    // Handle mySet >= highestSet case: player has already put in enough chips
    // AND has non-zero cash
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        // Player has sufficient chips and cash - call is valid if targetBet is within cash
        // (targetBet is irrelevant when mySet >= highestSet, but check for consistency)
        if (targetBet <= p.getMyCash()) {
            return 0;  // valid call
        }
    }
    // mySet < highestSet case handled below
    else if (p.getMyCash() > 0 && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (targetBet == p.getMyCash()) {
            return 0;
        }
    }
    else if (p.getMyCash() <= 0) {
        // No cash to call with
        return 1;
    }

    return 1;  // invalid
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction() PLAYER_ACTION_CALL myCash > 0 guard bug\n\n");

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

    // --- Scenario 2: Normal all-in case (myCash > 0) ---
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

    // --- Scenario 4: mySet < highestSet, myCash > 0, valid call ---
    printf("\n--- Scenario 4 (mySet=50, highestSet=100, myCash=75, targetBet=50) ---\n");
    MockPlayer p4;
    p4.mySet = 50;
    p4.myCash = 75;
    int fixed4 = checkMyActionCallFixed(p4, 50, 100);
    printf("  Fixed: %d (expected 0, valid call since 75+50>=100 and targetBet==50==100-50)\n", fixed4);
    if (fixed4 == 0) {
        printf("  PASS: Fixed code accepts valid call\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: checkMyAction myCash > 0 guard bug confirmed and fix verified\n");
    return 0;
}