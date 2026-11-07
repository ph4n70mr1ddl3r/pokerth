// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL mySet == 0 and highestSet == 0.
//
// Bug hunt: Verify PLAYER_ACTION_CALL correctly handles the case where
// mySet == 0 and highestSet == 0 (preflop small blind scenario).
// When a player has not yet put in any chips and highestSet is 0,
// a call should be valid if the player has sufficient cash.
//
// This test was inspired by recent bug fixes in checkMyAction.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_call_mySet_zero_highest_zero test_call_mySet_zero_highest_zero.cpp && /tmp/test_call_mySet_zero_highest_zero
//
#include <cstdio>
#include <cstdlib>
#include <cassert>

// Mock Player: minimal subset needed for the call test.
struct MockPlayer {
    int mySet{0};
    int myCash{1000};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed PLAYER_ACTION_CALL check: handles mySet >= highestSet case with cash guard
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    // Handle mySet >= highestSet case: player has already put in enough chips
    // (mySet >= highestSet), call is valid if targetBet is within cash AND player has non-zero cash
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        // Player has sufficient chips and cash - call is valid if targetBet is within cash
        if (targetBet <= p.getMyCash()) {
            return 0;  // valid call
        }
    }
    // mySet < highestSet case handled below, but only if player has non-zero cash
    else if(p.getMyCash() > 0 && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (p.getMyCash() + p.getMySet() <= highestSet && targetBet == p.getMyCash()) {
            return 0;
        }
    }

    return 1;  // invalid
}

// Old (buggy) version: missing cash guard or incorrect condition
static int checkMyActionCallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_CALL mySet == 0 and highestSet == 0\n\n");

    // --- Scenario 1: mySet == 0, highestSet == 0, myCash > 0, targetBet == 0 (THE BUG SCENARIO) ---
    printf("--- Scenario 1 (mySet=0, highestSet=0, myCash=1000, targetBet=0) ---\n");
    MockPlayer p1;
    p1.mySet = 0;
    p1.myCash = 1000;
    int fixed1 = checkMyActionCallFixed(p1, 0, 0);
    int buggy1 = checkMyActionCallBuggy(p1, 0, 0);
    printf("  Fixed code: %d (expected 0, valid call - posting small blind)\n", fixed1);
    printf("  Buggy code: %d\n", buggy1);
    if (fixed1 == 0) {
        printf("  PASS: Fixed code accepts call when mySet=0, highestSet=0, targetBet=0\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }
    if (buggy1 == 0) {
        printf("  Buggy code also accepts (expected behavior matches fixed)\n");
    } else {
        printf("  Buggy code rejects (unexpected)\n");
    }

    // --- Scenario 2: mySet == 0, highestSet == 0, myCash > 0, targetBet > 0 ---
    printf("\n--- Scenario 2 (mySet=0, highestSet=0, myCash=1000, targetBet=50) ---\n");
    MockPlayer p2;
    p2.mySet = 0;
    p2.myCash = 1000;
    int fixed2 = checkMyActionCallFixed(p2, 50, 0);
    printf("  Fixed code: %d (expected 0, valid call with 50 chips)\n", fixed2);
    if (fixed2 == 0) {
        printf("  PASS: Fixed code accepts call when targetBet=50 <= myCash=1000\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }

    // --- Scenario 3: mySet == 0, highestSet == 0, myCash > 0, targetBet > myCash ---
    printf("\n--- Scenario 3 (mySet=0, highestSet=0, myCash=100, targetBet=200) ---\n");
    MockPlayer p3;
    p3.mySet = 0;
    p3.myCash = 100;
    int fixed3 = checkMyActionCallFixed(p3, 200, 0);
    printf("  Fixed code: %d (expected 1, invalid since targetBet > myCash)\n", fixed3);
    if (fixed3 == 1) {
        printf("  PASS: Fixed code correctly rejects call when targetBet > myCash\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 4: mySet == 0, highestSet == 0, myCash == 0, targetBet == 0 ---
    printf("\n--- Scenario 4 (mySet=0, highestSet=0, myCash=0, targetBet=0) ---\n");
    MockPlayer p4;
    p4.mySet = 0;
    p4.myCash = 0;
    int fixed4 = checkMyActionCallFixed(p4, 0, 0);
    printf("  Fixed code: %d (expected 1, invalid - no cash)\n", fixed4);
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

    printf("TEST PASSED: PLAYER_ACTION_CALL mySet == 0 and highestSet == 0 edge case verified\n");
    return 0;
}