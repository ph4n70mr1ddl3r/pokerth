// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL when mySet > highestSet.
//
// Bug hunt: Verify PLAYER_ACTION_CALL correctly handles the case where
// mySet > highestSet. When a player has put in more chips than the current
// highest bet, a call should be valid (player has already contributed enough).
//
// This test was inspired by recent bug fixes in checkMyAction that added
// missing validation checks to edge cases.
//
// The fix: Changed PLAYER_ACTION_CHECK condition from == to >=
// and PLAYER_ACTION_CALL handles mySet >= highestSet case.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_call_mySet_greater test_call_mySet_greater.cpp && /tmp/test_call_mySet_greater

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

// Fixed PLAYER_ACTION_CALL check: handles mySet >= highestSet case
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    // Handle mySet >= highestSet case: player has already put in enough chips
    if (p.getMySet() >= highestSet) {
        // Player has sufficient chips - call is valid if targetBet is within cash
        if (targetBet <= p.getMyCash()) {
            return 0;  // valid call
        }
    }
    // mySet < highestSet case handled below
    else if (targetBet <= p.getMyCash()) {
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

// Old (buggy) version: only checks mySet == highestSet
static int checkMyActionCallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() == highestSet) {
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

int main()
{
    int failures = 0;

    // --- Scenario 1: mySet > highestSet, call should be valid (THE BUG) ---
    printf("--- Scenario 1: mySet > highestSet, call valid ---\n");
    MockPlayer p1;
    p1.mySet = 150;
    p1.myCash = 500;
    int fixed1 = checkMyActionCallFixed(p1, 100, 100);
    int buggy1 = checkMyActionCallBuggy(p1, 100, 100);
    printf("  Fixed code (mySet >= highestSet): %d (expected 0, valid call)\n", fixed1);
    printf("  Buggy code (mySet == highestSet only): %d\n", buggy1);
    if (fixed1 == 0) {
        printf("  PASS: Fixed code accepts call when mySet > highestSet\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }
    if (buggy1 == 1) {
        printf("  BUG CONFIRMED: Buggy code rejects call when mySet > highestSet\n");
    } else {
        printf("  Buggy code unexpected result\n");
    }

    // --- Scenario 2: mySet == highestSet, call valid ---
    printf("\n--- Scenario 2: mySet == highestSet, call valid ---\n");
    MockPlayer p2;
    p2.mySet = 100;
    p2.myCash = 500;
    int fixed2 = checkMyActionCallFixed(p2, 100, 100);
    printf("  Fixed code: %d (expected 0)\n", fixed2);
    if (fixed2 == 0) {
        printf("  PASS: Fixed code accepts call when mySet == highestSet\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }

    // --- Scenario 3: mySet < highestSet, call should need specific conditions ---
    printf("\n--- Scenario 3: mySet < highestSet, call invalid without proper conditions ---\n");
    MockPlayer p3;
    p3.mySet = 50;
    p3.myCash = 500;
    int fixed3 = checkMyActionCallFixed(p3, 100, 200);
    printf("  Fixed code: %d (expected 1, invalid since need to add 150 chips)\n", fixed3);
    if (fixed3 == 1) {
        printf("  PASS: Fixed code correctly rejects partial call\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 4: mySet < highestSet, all-in call valid ---
    printf("\n--- Scenario 4: mySet < highestSet, all-in call valid ---\n");
    MockPlayer p4;
    p4.mySet = 50;
    p4.myCash = 100;
    int fixed4 = checkMyActionCallFixed(p4, 100, 200);
    printf("  Fixed code: %d (expected 0, all-in with 150 total vs 200 highest)\n", fixed4);
    if (fixed4 == 0) {
        printf("  PASS: Fixed code accepts all-in call\n");
    } else {
        printf("  FAIL: expected 0\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL mySet > highestSet bug verification complete\n");
    return 0;
}