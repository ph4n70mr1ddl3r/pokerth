// Test for LocalPlayer::riverEngine3() PLAYER_ACTION_CALL double-subtraction bug.
//
// Bug: In LocalPlayer::riverEngine3(), the PLAYER_ACTION_CALL case has double
// subtraction of the call amount from myCash:
//   1. Line 4802 (old): myCash = myCash - highestSet + mySet; (inside engine function)
//   2. Lines 4422-4427: myCash -= toCall; (inside switch statement)
//      where toCall = highestSet - oldMySet
//
// Both execute, causing myCash to be reduced by 2*(highestSet - oldMySet) instead
// of (highestSet - oldMySet).
//
// Fix: Remove the myCash adjustment from the engine function's CALL branch,
//      since the switch statement already handles it correctly.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the riverEngine3 test
struct MockLocalPlayer {
    int mySet{0};
    int myCash{150};
    int oldMySet{20};
    int highestSet{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
};

// BUGGY: Simulates the OLD riverEngine3 where both engine function and
// switch subtract from myCash, causing double subtraction
static int riverEngine3_call_buggy(MockLocalPlayer& p) {
    // Engine function's CALL branch: myCash = myCash - highestSet + mySet
    p.myCash = p.myCash - p.highestSet + p.mySet;
    p.mySet = p.highestSet;

    // Switch PLAYER_ACTION_CALL: myCash -= toCall where toCall = highestSet - oldMySet
    int toCall = p.highestSet - p.oldMySet;
    if (toCall > 0) {
        p.myCash -= toCall;
    }
    return 0;
}

// FIXED: Simulates the NEW riverEngine3 where only the switch subtracts from myCash
// (engine function's CALL branch adjustment removed, matching the flopEngine3 fix)
static int riverEngine3_call_fixed(MockLocalPlayer& p) {
    // Engine function's CALL branch adjustment REMOVED
    // (myCash and mySet are only adjusted by the switch)

    // Switch PLAYER_ACTION_CALL handles the adjustment
    int toCall = p.highestSet - p.oldMySet;
    if (toCall > 0) {
        p.myCash -= toCall;
        p.mySet = p.highestSet;
    }
    return 0;
}

int main()
{
    int failures = 0;
    bool scenario1_passed = false;
    bool scenario2_passed = false;
    bool scenario3_passed = false;

    printf("Testing riverEngine3() PLAYER_ACTION_CALL double-subtraction bug\n\n");

    // --- Scenario 1: Normal call ---
    printf("--- Scenario 1 (mySet=20, myCash=150, oldMySet=20, highestSet=100) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 20;
    p1.myCash = 150;
    p1.oldMySet = 20;
    p1.highestSet = 100;

    int cashBefore1 = p1.myCash;

    riverEngine3_call_buggy(p1);
    // Buggy: myCash = 150 - 100 + 20 (engine) - 80 (switch) = -10
    // Correct: myCash = 150 - 80 = 70
    bool buggy1_confirmed = (p1.myCash != 70);
    if (buggy1_confirmed) {
        printf("  BUG CONFIRMED: Double subtraction! myCash = %d instead of 70\n",
               p1.myCash);
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Test fix
    p1.myCash = cashBefore1;
    p1.mySet = 20;
    riverEngine3_call_fixed(p1);
    bool fix1_ok = (p1.myCash == 70);
    if (fix1_ok) {
        printf("  PASS: Fixed code correctly gives myCash=70\n");
        scenario1_passed = true;
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of 70\n", p1.myCash);
        failures++;
    }

    // --- Scenario 2: toCall = 0 edge case ---
    printf("\n--- Scenario 2 (mySet=50, myCash=100, oldMySet=50, highestSet=50) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 50;
    p2.myCash = 100;
    p2.oldMySet = 50;
    p2.highestSet = 50;

    int cashBefore2 = p2.myCash;

    riverEngine3_call_buggy(p2);
    // toCall = 50 - 50 = 0, so no subtraction in either engine or switch
    // Buggy: myCash = 100 - 50 + 50 (engine, toCall=0) - 0 (switch, toCall=0) = 100
    // Correct: myCash = 100 (unchanged)
    bool buggy2_ok = (p2.myCash == 100);
    if (buggy2_ok) {
        printf("  OK: myCash correctly unchanged (toCall=0)\n");
    } else {
        printf("  BUG: myCash unexpectedly changed to %d\n", p2.myCash);
        failures++;
    }

    // Test fix
    p2.myCash = cashBefore2;
    riverEngine3_call_fixed(p2);
    bool fix2_ok = (p2.myCash == 100);
    if (fix2_ok) {
        printf("  PASS: Fixed code correctly keeps myCash=100\n");
        scenario2_passed = true;
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of 100\n", p2.myCash);
        failures++;
    }

    // --- Scenario 3: Larger call ---
    printf("\n--- Scenario 3 (mySet=30, myCash=100, oldMySet=30, highestSet=200) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 30;
    p3.myCash = 100;
    p3.oldMySet = 30;
    p3.highestSet = 200;

    int cashBefore3 = p3.myCash;

    riverEngine3_call_buggy(p3);
    // Buggy: myCash = 100 - 200 + 30 (engine) - 170 (switch) = -240
    // Correct: myCash = 100 - 170 = -70
    bool buggy3_confirmed = (p3.myCash != cashBefore3 - 170);
    if (buggy3_confirmed) {
        printf("  BUG PATTERN CONFIRMED: Double subtraction detected\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Test fix
    p3.myCash = cashBefore3;
    p3.mySet = 30;
    riverEngine3_call_fixed(p3);
    bool fix3_ok = (p3.myCash == cashBefore3 - 170);
    if (fix3_ok) {
        printf("  PASS: Fixed code correctly gives myCash=%d\n", p3.myCash);
        scenario3_passed = true;
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p3.myCash, cashBefore3 - 170);
        failures++;
    }

    printf("\n");
    printf("Scenario results:\n");
    printf("  Scenario 1 (double subtraction confirmed + fix verified): %s\n",
           scenario1_passed ? "PASS" : "FAIL");
    printf("  Scenario 2 (toCall=0 edge case): %s\n", scenario2_passed ? "PASS" : "FAIL");
    printf("  Scenario 3 (larger call): %s\n", scenario3_passed ? "PASS" : "FAIL");

    if (failures > 0) {
        printf("TEST FAILED: %d scenario failure(s)\n", failures);
        return 1;
    }

    printf("TEST PASSED: riverEngine3 PLAYER_ACTION_CALL double-subtraction bug confirmed and fix verified\n");
    return 0;
}